#ifndef SYSMON_H
#define SYSMON_H

#include "types.h"

// System Monitor - Real-time system monitoring module
// Inspired by Linux top/htop and Windows Task Manager

#define SYSMON_MAX_SAMPLES 64
#define SYSMON_SAMPLE_INTERVAL 100  // ticks

// CPU statistics
struct cpu_stats {
    uint64 idle_ticks;          // Time spent idle
    uint64 user_ticks;          // Time in user mode
    uint64 kernel_ticks;        // Time in kernel mode
    uint64 total_ticks;         // Total ticks
    int usage_percent;          // CPU usage percentage (0-100)
};

// Memory statistics
struct mem_stats {
    uint64 total_pages;         // Total physical pages
    uint64 free_pages;          // Free pages
    uint64 used_pages;          // Used pages
    uint64 kernel_pages;        // Pages used by kernel
    uint64 user_pages;          // Pages used by user processes
    int usage_percent;          // Memory usage percentage
};

// Process statistics
struct proc_stats {
    int pid;                    // Process ID
    char name[16];              // Process name
    int state;                  // Process state
    int priority;               // Process priority
    uint64 cpu_time;            // Total CPU time used
    uint64 mem_pages;           // Memory pages used
    uint64 start_time;          // Process start time
    int cpu_percent;            // CPU usage percentage
};

// System-wide statistics
struct system_stats {
    uint64 uptime;              // System uptime in ticks
    int num_processes;          // Number of processes
    int num_running;            // Running processes
    int num_sleeping;           // Sleeping processes
    int num_zombie;             // Zombie processes
    uint64 context_switches;    // Total context switches
    uint64 interrupts;          // Total interrupts
    uint64 syscalls;            // Total system calls
    struct cpu_stats cpu;       // CPU statistics
    struct mem_stats mem;       // Memory statistics
};

// Load average (1, 5, 15 minute averages)
struct load_avg {
    int load_1min;              // 1 minute load (x100)
    int load_5min;              // 5 minute load (x100)
    int load_15min;             // 15 minute load (x100)
};

// System monitor functions
void sysmon_init(void);
void sysmon_update(void);
void sysmon_get_system_stats(struct system_stats *stats);
void sysmon_get_proc_stats(int pid, struct proc_stats *stats);
void sysmon_get_load_avg(struct load_avg *load);
void sysmon_print_summary(void);
void sysmon_print_processes(void);
int sysmon_get_cpu_usage(void);
int sysmon_get_mem_usage(void);

// Performance counters
void sysmon_inc_syscalls(void);
void sysmon_inc_interrupts(void);
void sysmon_inc_context_switches(void);

// Global system monitor instance
extern struct system_stats sysmon_stats;

#endif // SYSMON_H
