#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"
#include "sysmon.h"

// System Monitor Implementation
// Provides real-time system monitoring similar to Linux top/htop

// Global statistics
struct system_stats sysmon_stats;
static struct spinlock sysmon_lock;

// Performance counters
static uint64 total_syscalls = 0;
static uint64 total_interrupts = 0;
static uint64 total_ctx_switches = 0;

// Load average samples
static int load_samples[SYSMON_MAX_SAMPLES];
static int load_sample_idx = 0;
static uint64 last_sample_time = 0;

// Previous tick counts for CPU usage calculation
static uint64 prev_idle_ticks = 0;
static uint64 prev_total_ticks = 0;

// Initialize system monitor
void sysmon_init(void) {
    initlock(&sysmon_lock, "sysmon");
    
    // Initialize statistics
    sysmon_stats.uptime = 0;
    sysmon_stats.num_processes = 0;
    sysmon_stats.num_running = 0;
    sysmon_stats.num_sleeping = 0;
    sysmon_stats.num_zombie = 0;
    sysmon_stats.context_switches = 0;
    sysmon_stats.interrupts = 0;
    sysmon_stats.syscalls = 0;
    
    // Initialize CPU stats
    sysmon_stats.cpu.idle_ticks = 0;
    sysmon_stats.cpu.user_ticks = 0;
    sysmon_stats.cpu.kernel_ticks = 0;
    sysmon_stats.cpu.total_ticks = 0;
    sysmon_stats.cpu.usage_percent = 0;
    
    // Initialize memory stats
    sysmon_stats.mem.total_pages = 0;
    sysmon_stats.mem.free_pages = 0;
    sysmon_stats.mem.used_pages = 0;
    sysmon_stats.mem.kernel_pages = 0;
    sysmon_stats.mem.user_pages = 0;
    sysmon_stats.mem.usage_percent = 0;
    
    // Initialize load samples
    for (int i = 0; i < SYSMON_MAX_SAMPLES; i++) {
        load_samples[i] = 0;
    }
    
    printf("[SysMon] System monitor initialized\n");
}

// Update system statistics (called periodically)
void sysmon_update(void) {
    acquire(&sysmon_lock);
    
    // Update uptime
    sysmon_stats.uptime = ticks;
    
    // Update performance counters
    sysmon_stats.syscalls = total_syscalls;
    sysmon_stats.interrupts = total_interrupts;
    sysmon_stats.context_switches = total_ctx_switches;
    
    // Count processes by state
    int running = 0, sleeping = 0, zombie = 0, total = 0;
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p->state != UNUSED) {
            total++;
            if (p->state == RUNNING || p->state == RUNNABLE) {
                running++;
            } else if (p->state == SLEEPING) {
                sleeping++;
            } else if (p->state == ZOMBIE) {
                zombie++;
            }
        }
    }
    
    sysmon_stats.num_processes = total;
    sysmon_stats.num_running = running;
    sysmon_stats.num_sleeping = sleeping;
    sysmon_stats.num_zombie = zombie;
    
    // Calculate CPU usage
    uint64 current_total = ticks;
    uint64 delta_total = current_total - prev_total_ticks;
    
    if (delta_total > 0) {
        // Estimate idle time based on running processes
        uint64 busy_estimate = running * delta_total / NCPU;
        if (busy_estimate > delta_total) busy_estimate = delta_total;
        
        sysmon_stats.cpu.usage_percent = (int)((busy_estimate * 100) / delta_total);
        if (sysmon_stats.cpu.usage_percent > 100) {
            sysmon_stats.cpu.usage_percent = 100;
        }
    }
    
    prev_total_ticks = current_total;
    sysmon_stats.cpu.total_ticks = current_total;
    
    // Update load average sample
    if (ticks - last_sample_time >= SYSMON_SAMPLE_INTERVAL) {
        load_samples[load_sample_idx] = running;
        load_sample_idx = (load_sample_idx + 1) % SYSMON_MAX_SAMPLES;
        last_sample_time = ticks;
    }
    
    release(&sysmon_lock);
}

// Get system statistics
void sysmon_get_system_stats(struct system_stats *stats) {
    if (!stats) return;
    
    sysmon_update();
    
    acquire(&sysmon_lock);
    stats->uptime = sysmon_stats.uptime;
    stats->num_processes = sysmon_stats.num_processes;
    stats->num_running = sysmon_stats.num_running;
    stats->num_sleeping = sysmon_stats.num_sleeping;
    stats->num_zombie = sysmon_stats.num_zombie;
    stats->context_switches = sysmon_stats.context_switches;
    stats->interrupts = sysmon_stats.interrupts;
    stats->syscalls = sysmon_stats.syscalls;
    stats->cpu = sysmon_stats.cpu;
    stats->mem = sysmon_stats.mem;
    release(&sysmon_lock);
}

// Get process statistics
void sysmon_get_proc_stats(int pid, struct proc_stats *stats) {
    if (!stats) return;
    
    struct proc *p;
    
    acquire(&sysmon_lock);
    
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p->pid == pid && p->state != UNUSED) {
            stats->pid = p->pid;
            // Copy process name
            for (int i = 0; i < 16 && p->name[i]; i++) {
                stats->name[i] = p->name[i];
            }
            stats->state = p->state;
            stats->priority = 0; // Default priority
            stats->cpu_time = 0; // Would need per-process tracking
            stats->mem_pages = p->sz / PGSIZE;
            stats->start_time = 0;
            stats->cpu_percent = 0;
            break;
        }
    }
    
    release(&sysmon_lock);
}

// Get load average
void sysmon_get_load_avg(struct load_avg *load) {
    if (!load) return;
    
    acquire(&sysmon_lock);
    
    // Calculate 1-minute average (last ~6 samples at 100 tick interval)
    int sum1 = 0, count1 = 0;
    for (int i = 0; i < 6 && i < SYSMON_MAX_SAMPLES; i++) {
        int idx = (load_sample_idx - 1 - i + SYSMON_MAX_SAMPLES) % SYSMON_MAX_SAMPLES;
        sum1 += load_samples[idx];
        count1++;
    }
    load->load_1min = count1 > 0 ? (sum1 * 100) / count1 : 0;
    
    // Calculate 5-minute average (last ~30 samples)
    int sum5 = 0, count5 = 0;
    for (int i = 0; i < 30 && i < SYSMON_MAX_SAMPLES; i++) {
        int idx = (load_sample_idx - 1 - i + SYSMON_MAX_SAMPLES) % SYSMON_MAX_SAMPLES;
        sum5 += load_samples[idx];
        count5++;
    }
    load->load_5min = count5 > 0 ? (sum5 * 100) / count5 : 0;
    
    // Calculate 15-minute average (all samples)
    int sum15 = 0, count15 = 0;
    for (int i = 0; i < SYSMON_MAX_SAMPLES; i++) {
        sum15 += load_samples[i];
        count15++;
    }
    load->load_15min = count15 > 0 ? (sum15 * 100) / count15 : 0;
    
    release(&sysmon_lock);
}

// Print system summary
void sysmon_print_summary(void) {
    sysmon_update();
    
    acquire(&sysmon_lock);
    
    printf("\n============ System Monitor ============\n");
    printf("Uptime: %d ticks (%d seconds)\n", 
           (int)sysmon_stats.uptime, (int)(sysmon_stats.uptime / 100));
    printf("\nProcesses: %d total, %d running, %d sleeping, %d zombie\n",
           sysmon_stats.num_processes,
           sysmon_stats.num_running,
           sysmon_stats.num_sleeping,
           sysmon_stats.num_zombie);
    printf("CPU Usage: %d%%\n", sysmon_stats.cpu.usage_percent);
    printf("\nPerformance Counters:\n");
    printf("  System calls:     %d\n", (int)sysmon_stats.syscalls);
    printf("  Interrupts:       %d\n", (int)sysmon_stats.interrupts);
    printf("  Context switches: %d\n", (int)sysmon_stats.context_switches);
    printf("========================================\n\n");
    
    release(&sysmon_lock);
}

// Print process list
void sysmon_print_processes(void) {
    struct proc *p;
    
    printf("\n  PID  STATE      NAME\n");
    printf("  ---  -----      ----\n");
    
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p->state != UNUSED) {
            char *state;
            switch (p->state) {
                case SLEEPING: state = "SLEEP "; break;
                case RUNNABLE: state = "READY "; break;
                case RUNNING:  state = "RUN   "; break;
                case ZOMBIE:   state = "ZOMBIE"; break;
                default:       state = "???   "; break;
            }
            printf("  %d    %s     %s\n", p->pid, state, p->name);
        }
    }
    printf("\n");
}

// Get CPU usage percentage
int sysmon_get_cpu_usage(void) {
    sysmon_update();
    return sysmon_stats.cpu.usage_percent;
}

// Get memory usage percentage
int sysmon_get_mem_usage(void) {
    return sysmon_stats.mem.usage_percent;
}

// Increment syscall counter
void sysmon_inc_syscalls(void) {
    __sync_fetch_and_add(&total_syscalls, 1);
}

// Increment interrupt counter
void sysmon_inc_interrupts(void) {
    __sync_fetch_and_add(&total_interrupts, 1);
}

// Increment context switch counter
void sysmon_inc_context_switches(void) {
    __sync_fetch_and_add(&total_ctx_switches, 1);
}
