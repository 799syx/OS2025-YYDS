#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"
#include "embassy.h"

// Forward declarations for static functions
static void embassy_add_to_ready_queue_locked(struct embassy_task *task);
static void embassy_add_to_ready_queue(struct embassy_task *task);
static struct embassy_task* embassy_get_next_task(void);
static void embassy_update_waiting_tasks(void);
static struct embassy_task* embassy_find_task(int task_id);
static void embassy_check_dependencies(int completed_task_id);
static void embassy_periodic_priority_boost(void);

// Global scheduler instance
struct embassy_scheduler embassy_sched;

// Static lock storage
static struct spinlock embassy_lock;

// String copy helper
static void embassy_strncpy(char *dst, const char *src, int n) {
    int i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

// Embassy initialization
void embassy_init(void) {
    printf("[Embassy] Initializing async scheduler...\n");
    
    // Initialize scheduler lock
    embassy_sched.lock = &embassy_lock;
    initlock(embassy_sched.lock, "embassy_sched");
    
    // Initialize ready queues
    for (int i = 0; i < 4; i++) {
        embassy_sched.ready_queues[i] = 0;
    }
    
    embassy_sched.waiting_queue = 0;
    embassy_sched.blocked_queue = 0;
    embassy_sched.current_task = 0;
    embassy_sched.task_counter = 0;
    embassy_sched.time_slice = 10;
    
    // Initialize global stats
    embassy_sched.global_stats.total_tasks_created = 0;
    embassy_sched.global_stats.total_tasks_completed = 0;
    embassy_sched.global_stats.total_tasks_failed = 0;
    embassy_sched.global_stats.current_active_tasks = 0;
    embassy_sched.global_stats.total_context_switches = 0;
    embassy_sched.global_stats.total_priority_boosts = 0;
    embassy_sched.global_stats.scheduler_invocations = 0;
    
    // Priority boost config
    embassy_sched.priority_boost_interval = 100;
    embassy_sched.last_boost_time = 0;
    
    printf("[Embassy] Scheduler initialized (timeslice=%dms, boost_interval=%d ticks)\n", 
           embassy_sched.time_slice, embassy_sched.priority_boost_interval);
}

// Create task with name
int embassy_create_task_named(void (*async_func)(void *), void *arg, int priority, const char *name) {
    struct embassy_task *task;
    
    // Check task limit
    if (embassy_sched.global_stats.current_active_tasks >= EMBASSY_MAX_TASKS) {
        printf("[Embassy] Error: max task limit reached (%d)\n", EMBASSY_MAX_TASKS);
        return -1;
    }
    
    // Allocate task memory
    task = (struct embassy_task*)kalloc();
    if (!task) {
        printf("[Embassy] Error: failed to allocate task memory\n");
        embassy_sched.global_stats.total_tasks_failed++;
        return -1;
    }
    
    // Initialize task
    task->task_id = embassy_sched.task_counter++;
    task->state = EMBASSY_READY;
    task->priority = (enum embassy_priority)priority;
    task->base_priority = (enum embassy_priority)priority;
    task->async_func = async_func;
    task->arg = arg;
    task->bound_proc = myproc();
    task->wake_time = 0;
    task->timeout = 0;
    task->next = 0;
    
    // Initialize task stats
    task->stats.create_time = ticks;
    task->stats.start_time = 0;
    task->stats.end_time = 0;
    task->stats.total_run_time = 0;
    task->stats.total_wait_time = 0;
    task->stats.yield_count = 0;
    task->stats.priority_boosts = 0;
    task->stats.state_changes = 0;
    
    // Initialize task dependencies
    task->dep_count = 0;
    task->completed_deps = 0;
    for (int i = 0; i < EMBASSY_MAX_DEPS; i++) {
        task->dep_task_ids[i] = -1;
    }
    
    // Initialize task labels
    if (name) {
        embassy_strncpy(task->name, name, 16);
    } else {
        task->name[0] = '\0';
    }
    task->group_id = 0;
    
    // Add to ready queue
    embassy_add_to_ready_queue(task);
    
    // Update global stats
    embassy_sched.global_stats.total_tasks_created++;
    embassy_sched.global_stats.current_active_tasks++;
    
    printf("[Embassy] Created task %d '%s' (priority=%d)\n", task->task_id, 
           task->name[0] ? task->name : "unnamed", priority);
    return task->task_id;
}

// Create async task (compatible interface)
int embassy_create_task(void (*async_func)(void *), void *arg, int priority) {
    return embassy_create_task_named(async_func, arg, priority, 0);
}

// Destroy async task
void embassy_destroy_task(int task_id) {
    struct embassy_task *task, *prev;
    
    acquire(embassy_sched.lock);
    
    // Search in ready queues
    for (int i = 0; i < 4; i++) {
        task = embassy_sched.ready_queues[i];
        prev = 0;
        
        while (task) {
            if (task->task_id == task_id) {
                if (prev) {
                    prev->next = task->next;
                } else {
                    embassy_sched.ready_queues[i] = task->next;
                }
                
                task->stats.end_time = ticks;
                embassy_sched.global_stats.total_tasks_completed++;
                embassy_sched.global_stats.current_active_tasks--;
                
                embassy_check_dependencies(task_id);
                
                kfree(task);
                release(embassy_sched.lock);
                printf("[Embassy] Destroyed task %d\n", task_id);
                return;
            }
            prev = task;
            task = task->next;
        }
    }
    
    // Search in waiting queue
    task = embassy_sched.waiting_queue;
    prev = 0;
    while (task) {
        if (task->task_id == task_id) {
            if (prev) {
                prev->next = task->next;
            } else {
                embassy_sched.waiting_queue = task->next;
            }
            embassy_sched.global_stats.current_active_tasks--;
            embassy_check_dependencies(task_id);
            kfree(task);
            release(embassy_sched.lock);
            printf("[Embassy] Destroyed task %d\n", task_id);
            return;
        }
        prev = task;
        task = task->next;
    }
    
    // Search in blocked queue
    task = embassy_sched.blocked_queue;
    prev = 0;
    while (task) {
        if (task->task_id == task_id) {
            if (prev) {
                prev->next = task->next;
            } else {
                embassy_sched.blocked_queue = task->next;
            }
            embassy_sched.global_stats.current_active_tasks--;
            kfree(task);
            release(embassy_sched.lock);
            printf("[Embassy] Destroyed task %d\n", task_id);
            return;
        }
        prev = task;
        task = task->next;
    }
    
    release(embassy_sched.lock);
    printf("[Embassy] Task %d not found\n", task_id);
}

// Find task (must hold lock)
static struct embassy_task* embassy_find_task(int task_id) {
    struct embassy_task *task;
    
    for (int i = 0; i < 4; i++) {
        task = embassy_sched.ready_queues[i];
        while (task) {
            if (task->task_id == task_id) {
                return task;
            }
            task = task->next;
        }
    }
    
    task = embassy_sched.waiting_queue;
    while (task) {
        if (task->task_id == task_id) {
            return task;
        }
        task = task->next;
    }
    
    task = embassy_sched.blocked_queue;
    while (task) {
        if (task->task_id == task_id) {
            return task;
        }
        task = task->next;
    }
    
    return 0;
}

// Check and update dependent tasks
static void embassy_check_dependencies(int completed_task_id) {
    struct embassy_task *task, *prev, *next;
    
    task = embassy_sched.blocked_queue;
    prev = 0;
    
    while (task) {
        next = task->next;
        int found = 0;
        
        for (int i = 0; i < task->dep_count; i++) {
            if (task->dep_task_ids[i] == completed_task_id) {
                task->completed_deps++;
                task->dep_task_ids[i] = -1;
                found = 1;
                break;
            }
        }
        
        if (found && task->completed_deps >= task->dep_count) {
            if (prev) {
                prev->next = next;
            } else {
                embassy_sched.blocked_queue = next;
            }
            task->next = 0;
            embassy_add_to_ready_queue_locked(task);
            printf("[Embassy] Task %d dependencies satisfied, moved to ready\n", task->task_id);
            task = next;
        } else {
            prev = task;
            task = next;
        }
    }
}

// Periodic priority boost (prevent starvation)
static void embassy_periodic_priority_boost(void) {
    uint64 current_time = ticks;
    
    if (current_time - embassy_sched.last_boost_time < embassy_sched.priority_boost_interval) {
        return;
    }
    
    embassy_sched.last_boost_time = current_time;
    
    acquire(embassy_sched.lock);
    
    for (int i = 3; i > 0; i--) {
        struct embassy_task *task = embassy_sched.ready_queues[i];
        while (task) {
            struct embassy_task *next = task->next;
            
            if (task->priority > EMBASSY_CRITICAL) {
                task->priority = (enum embassy_priority)(task->priority - 1);
                task->stats.priority_boosts++;
                embassy_sched.global_stats.total_priority_boosts++;
            }
            
            task = next;
        }
    }
    
    release(embassy_sched.lock);
}

// Add task to ready queue (internal, caller holds lock)
static void embassy_add_to_ready_queue_locked(struct embassy_task *task) {
    int priority = (int)task->priority;
    if (priority < 0 || priority >= 4) {
        priority = EMBASSY_NORMAL;
    }
    
    task->next = 0;
    if (!embassy_sched.ready_queues[priority]) {
        embassy_sched.ready_queues[priority] = task;
    } else {
        struct embassy_task *current = embassy_sched.ready_queues[priority];
        while (current->next) {
            current = current->next;
        }
        current->next = task;
    }
    
    task->state = EMBASSY_READY;
    task->stats.state_changes++;
}

// Add task to ready queue
static void embassy_add_to_ready_queue(struct embassy_task *task) {
    acquire(embassy_sched.lock);
    embassy_add_to_ready_queue_locked(task);
    release(embassy_sched.lock);
}

// Get next task to run
static struct embassy_task* embassy_get_next_task(void) {
    acquire(embassy_sched.lock);
    
    struct embassy_task *task = 0;
    
    for (int i = 0; i < 4; i++) {
        if (embassy_sched.ready_queues[i]) {
            task = embassy_sched.ready_queues[i];
            embassy_sched.ready_queues[i] = task->next;
            task->next = 0;
            task->state = EMBASSY_RUNNING;
            task->stats.state_changes++;
            break;
        }
    }
    
    embassy_sched.current_task = task;
    release(embassy_sched.lock);
    return task;
}

// Update waiting tasks
static void embassy_update_waiting_tasks(void) {
    uint64 current_time = ticks;
    struct embassy_task *task, *prev, *next;
    
    acquire(embassy_sched.lock);
    
    task = embassy_sched.waiting_queue;
    prev = 0;
    
    while (task) {
        next = task->next;
        
        if (current_time >= task->wake_time) {
            if (prev) {
                prev->next = next;
            } else {
                embassy_sched.waiting_queue = next;
            }
            
            embassy_add_to_ready_queue_locked(task);
            task = next;
        } else {
            prev = task;
            task = next;
        }
    }
    
    release(embassy_sched.lock);
}

// Embassy main scheduler
void embassy_schedule(void) {
    struct embassy_task *task;
    
    embassy_periodic_priority_boost();
    embassy_update_waiting_tasks();
    
    embassy_sched.global_stats.scheduler_invocations++;
    
    task = embassy_get_next_task();
    
    if (task) {
        printf("[Embassy] Scheduling task %d\n", task->task_id);
        
        task->stats.start_time = ticks;
        embassy_sched.global_stats.total_context_switches++;
        
        if (task->async_func) {
            task->async_func(task->arg);
        }
        
        task->state = EMBASSY_COMPLETED;
        task->stats.end_time = ticks;
        task->stats.total_run_time = task->stats.end_time - task->stats.start_time;
        
        embassy_destroy_task(task->task_id);
    }
}

// Task yield CPU
void embassy_yield(void) {
    if (embassy_sched.current_task) {
        embassy_sched.current_task->stats.yield_count++;
        embassy_sched.current_task->state = EMBASSY_READY;
        embassy_add_to_ready_queue(embassy_sched.current_task);
        embassy_sched.current_task = 0;
    }
    yield();
}

// Delay function
void embassy_delay_ms(int milliseconds) {
    if (embassy_sched.current_task) {
        embassy_sched.current_task->wake_time = ticks + (milliseconds * 10) / 1000;
        embassy_sched.current_task->state = EMBASSY_WAITING;
        embassy_sched.current_task->stats.state_changes++;
        
        acquire(embassy_sched.lock);
        embassy_sched.current_task->next = embassy_sched.waiting_queue;
        embassy_sched.waiting_queue = embassy_sched.current_task;
        release(embassy_sched.lock);
        
        embassy_sched.current_task = 0;
    }
    yield();
}

// Wait for event
void embassy_wait_event(int event_id) {
    embassy_delay_ms(100);
}

// Trigger event
void embassy_trigger_event(int event_id) {
    printf("[Embassy] Event %d triggered\n", event_id);
}

// Add task dependency
int embassy_add_dependency(int task_id, int dep_task_id) {
    acquire(embassy_sched.lock);
    
    struct embassy_task *task = embassy_find_task(task_id);
    if (!task) {
        release(embassy_sched.lock);
        printf("[Embassy] Error: task %d not found\n", task_id);
        return -1;
    }
    
    if (task->dep_count >= EMBASSY_MAX_DEPS) {
        release(embassy_sched.lock);
        printf("[Embassy] Error: task %d max deps reached\n", task_id);
        return -1;
    }
    
    struct embassy_task *dep_task = embassy_find_task(dep_task_id);
    if (!dep_task) {
        release(embassy_sched.lock);
        printf("[Embassy] Error: dep task %d not found\n", dep_task_id);
        return -1;
    }
    
    task->dep_task_ids[task->dep_count++] = dep_task_id;
    
    if (task->state == EMBASSY_READY) {
        int priority = (int)task->priority;
        struct embassy_task *curr = embassy_sched.ready_queues[priority];
        struct embassy_task *prev = 0;
        
        while (curr) {
            if (curr->task_id == task_id) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    embassy_sched.ready_queues[priority] = curr->next;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        
        task->state = EMBASSY_BLOCKED;
        task->next = embassy_sched.blocked_queue;
        embassy_sched.blocked_queue = task;
    }
    
    release(embassy_sched.lock);
    printf("[Embassy] Task %d depends on task %d\n", task_id, dep_task_id);
    return 0;
}

// Set task group
int embassy_set_task_group(int task_id, int group_id) {
    acquire(embassy_sched.lock);
    
    struct embassy_task *task = embassy_find_task(task_id);
    if (!task) {
        release(embassy_sched.lock);
        return -1;
    }
    
    task->group_id = group_id;
    release(embassy_sched.lock);
    return 0;
}

// Boost task priority
void embassy_boost_priority(int task_id) {
    acquire(embassy_sched.lock);
    
    struct embassy_task *task = embassy_find_task(task_id);
    if (task && task->priority > EMBASSY_CRITICAL) {
        task->priority = (enum embassy_priority)(task->priority - 1);
        task->stats.priority_boosts++;
        embassy_sched.global_stats.total_priority_boosts++;
        printf("[Embassy] Task %d priority boosted to %d\n", task_id, task->priority);
    }
    
    release(embassy_sched.lock);
}

// Get task stats
void embassy_get_task_stats(int task_id, struct embassy_task_stats *stats) {
    acquire(embassy_sched.lock);
    
    struct embassy_task *task = embassy_find_task(task_id);
    if (task && stats) {
        stats->create_time = task->stats.create_time;
        stats->start_time = task->stats.start_time;
        stats->end_time = task->stats.end_time;
        stats->total_run_time = task->stats.total_run_time;
        stats->total_wait_time = task->stats.total_wait_time;
        stats->yield_count = task->stats.yield_count;
        stats->priority_boosts = task->stats.priority_boosts;
        stats->state_changes = task->stats.state_changes;
    }
    
    release(embassy_sched.lock);
}

// Get global stats
void embassy_get_global_stats(struct embassy_global_stats *stats) {
    if (stats) {
        acquire(embassy_sched.lock);
        stats->total_tasks_created = embassy_sched.global_stats.total_tasks_created;
        stats->total_tasks_completed = embassy_sched.global_stats.total_tasks_completed;
        stats->total_tasks_failed = embassy_sched.global_stats.total_tasks_failed;
        stats->current_active_tasks = embassy_sched.global_stats.current_active_tasks;
        stats->total_context_switches = embassy_sched.global_stats.total_context_switches;
        stats->total_priority_boosts = embassy_sched.global_stats.total_priority_boosts;
        stats->scheduler_invocations = embassy_sched.global_stats.scheduler_invocations;
        release(embassy_sched.lock);
    }
}

// Print scheduler stats
void embassy_print_stats(void) {
    acquire(embassy_sched.lock);
    
    printf("\n========== Embassy Scheduler Stats ==========\n");
    printf("Total tasks created:    %d\n", embassy_sched.global_stats.total_tasks_created);
    printf("Total tasks completed:  %d\n", embassy_sched.global_stats.total_tasks_completed);
    printf("Total tasks failed:     %d\n", embassy_sched.global_stats.total_tasks_failed);
    printf("Current active tasks:   %d\n", embassy_sched.global_stats.current_active_tasks);
    printf("Total context switches: %d\n", (int)embassy_sched.global_stats.total_context_switches);
    printf("Total priority boosts:  %d\n", (int)embassy_sched.global_stats.total_priority_boosts);
    printf("Scheduler invocations:  %d\n", (int)embassy_sched.global_stats.scheduler_invocations);
    printf("=============================================\n\n");
    
    release(embassy_sched.lock);
}
