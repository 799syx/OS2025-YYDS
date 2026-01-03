#ifndef EMBASSY_H
#define EMBASSY_H

#include "types.h"

// Embassy priority levels
enum embassy_priority {
    EMBASSY_CRITICAL = 0,
    EMBASSY_HIGH = 1,
    EMBASSY_NORMAL = 2,
    EMBASSY_LOW = 3
};

// Embassy task states
enum embassy_state {
    EMBASSY_READY = 0,
    EMBASSY_RUNNING = 1,
    EMBASSY_WAITING = 2,
    EMBASSY_BLOCKED = 3,
    EMBASSY_COMPLETED = 4
};

// Constants
#define EMBASSY_MAX_TASKS 64
#define EMBASSY_MAX_DEPS 8
#define EMBASSY_MAX_NAME_LEN 16

// Task statistics
struct embassy_task_stats {
    uint64 create_time;
    uint64 start_time;
    uint64 end_time;
    uint64 total_run_time;
    uint64 total_wait_time;
    int yield_count;
    int priority_boosts;
    int state_changes;
};

// Embassy task structure
struct embassy_task {
    int task_id;
    enum embassy_state state;
    enum embassy_priority priority;
    enum embassy_priority base_priority;
    void (*async_func)(void *);
    void *arg;
    struct proc *bound_proc;
    uint64 wake_time;
    uint64 timeout;
    struct embassy_task *next;
    
    // Task statistics
    struct embassy_task_stats stats;
    
    // Dependencies
    int dep_count;
    int completed_deps;
    int dep_task_ids[EMBASSY_MAX_DEPS];
    
    // Task metadata
    char name[EMBASSY_MAX_NAME_LEN];
    int group_id;
};

// Global statistics
struct embassy_global_stats {
    int total_tasks_created;
    int total_tasks_completed;
    int total_tasks_failed;
    int current_active_tasks;
    uint64 total_context_switches;
    uint64 total_priority_boosts;
    uint64 scheduler_invocations;
};

// Embassy scheduler structure
struct embassy_scheduler {
    // Priority queues (4 levels)
    struct embassy_task *ready_queues[4];
    struct embassy_task *waiting_queue;
    struct embassy_task *blocked_queue;
    struct embassy_task *current_task;
    
    // Scheduler configuration
    int task_counter;
    int time_slice;
    struct spinlock *lock;
    
    // Priority boost configuration
    int priority_boost_interval;
    uint64 last_boost_time;
    
    // Global statistics
    struct embassy_global_stats global_stats;
};

// Function declarations
void embassy_init(void);
int embassy_create_task(void (*async_func)(void *), void *arg, int priority);
int embassy_create_task_named(void (*async_func)(void *), void *arg, int priority, const char *name);
void embassy_destroy_task(int task_id);
void embassy_schedule(void);
void embassy_yield(void);
void embassy_delay_ms(int milliseconds);
void embassy_wait_event(int event_id);
void embassy_trigger_event(int event_id);
int embassy_add_dependency(int task_id, int dep_task_id);
int embassy_set_task_group(int task_id, int group_id);
void embassy_boost_priority(int task_id);
void embassy_get_task_stats(int task_id, struct embassy_task_stats *stats);
void embassy_get_global_stats(struct embassy_global_stats *stats);
void embassy_print_stats(void);

#endif // EMBASSY_H
