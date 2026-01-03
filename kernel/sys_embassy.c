#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"
#include "proc/embassy.h"

// Embassy system call implementations

// Create async task
uint64 sys_embassy_create_task(void) {
    uint64 func_ptr;
    uint64 arg_ptr;
    int priority;
    
    if (argaddr(0, &func_ptr) < 0 || argaddr(1, &arg_ptr) < 0 || argint(2, &priority) < 0) {
        return -1;
    }
    
    // Convert function pointer from user space
    void (*async_func)(void *) = (void (*)(void *))func_ptr;
    void *arg = (void *)arg_ptr;
    
    return embassy_create_task(async_func, arg, priority);
}

// Create named async task
uint64 sys_embassy_create_task_named(void) {
    uint64 func_ptr;
    uint64 arg_ptr;
    int priority;
    uint64 name_ptr;
    char name[EMBASSY_MAX_NAME_LEN];
    
    if (argaddr(0, &func_ptr) < 0 || argaddr(1, &arg_ptr) < 0 || 
        argint(2, &priority) < 0 || argaddr(3, &name_ptr) < 0) {
        return -1;
    }
    
    // Convert function pointer from user space
    void (*async_func)(void *) = (void (*)(void *))func_ptr;
    void *arg = (void *)arg_ptr;
    
    // Copy task name from user space
    if (name_ptr != 0) {
        if (fetchstr(name_ptr, name, EMBASSY_MAX_NAME_LEN) < 0) {
            return -1;
        }
    }
    
    return embassy_create_task_named(async_func, arg, priority, name_ptr != 0 ? name : 0);
}

// Destroy async task
uint64 sys_embassy_destroy_task(void) {
    int task_id;
    
    if (argint(0, &task_id) < 0) {
        return -1;
    }
    
    embassy_destroy_task(task_id);
    return 0;
}

// Run embassy scheduler
uint64 sys_embassy_schedule(void) {
    embassy_schedule();
    return 0;
}

// Yield CPU
uint64 sys_embassy_yield(void) {
    embassy_yield();
    return 0;
}

// Delay for specified milliseconds
uint64 sys_embassy_delay_ms(void) {
    int milliseconds;
    
    if (argint(0, &milliseconds) < 0) {
        return -1;
    }
    
    embassy_delay_ms(milliseconds);
    return 0;
}

// Wait for event
uint64 sys_embassy_wait_event(void) {
    int event_id;
    
    if (argint(0, &event_id) < 0) {
        return -1;
    }
    
    embassy_wait_event(event_id);
    return 0;
}

// Trigger event
uint64 sys_embassy_trigger_event(void) {
    int event_id;
    
    if (argint(0, &event_id) < 0) {
        return -1;
    }
    
    embassy_trigger_event(event_id);
    return 0;
}

// Add task dependency
uint64 sys_embassy_add_dependency(void) {
    int task_id;
    int dep_task_id;
    
    if (argint(0, &task_id) < 0 || argint(1, &dep_task_id) < 0) {
        return -1;
    }
    
    return embassy_add_dependency(task_id, dep_task_id);
}

// Set task group
uint64 sys_embassy_set_task_group(void) {
    int task_id;
    int group_id;
    
    if (argint(0, &task_id) < 0 || argint(1, &group_id) < 0) {
        return -1;
    }
    
    return embassy_set_task_group(task_id, group_id);
}

// Boost task priority
uint64 sys_embassy_boost_priority(void) {
    int task_id;
    
    if (argint(0, &task_id) < 0) {
        return -1;
    }
    
    embassy_boost_priority(task_id);
    return 0;
}

// Get task statistics
uint64 sys_embassy_get_task_stats(void) {
    int task_id;
    uint64 stats_ptr;
    
    if (argint(0, &task_id) < 0 || argaddr(1, &stats_ptr) < 0) {
        return -1;
    }
    
    struct embassy_task_stats stats;
    embassy_get_task_stats(task_id, &stats);
    
    // Copy stats to user space
    if (copyout(myproc()->pagetable, stats_ptr, (char*)&stats, sizeof(stats)) < 0) {
        return -1;
    }
    
    return 0;
}

// Get global statistics
uint64 sys_embassy_get_global_stats(void) {
    uint64 stats_ptr;
    
    if (argaddr(0, &stats_ptr) < 0) {
        return -1;
    }
    
    struct embassy_global_stats stats;
    embassy_get_global_stats(&stats);
    
    // Copy stats to user space
    if (copyout(myproc()->pagetable, stats_ptr, (char*)&stats, sizeof(stats)) < 0) {
        return -1;
    }
    
    return 0;
}

// Print statistics
uint64 sys_embassy_print_stats(void) {
    embassy_print_stats();
    return 0;
}
