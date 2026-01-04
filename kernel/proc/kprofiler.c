// kprofiler.c - 内核性能统计 (Kernel Profiler)
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);
extern uint ticks;

#define PERF_SCHED_SWITCH   0
#define PERF_SYSCALL        1
#define PERF_PAGE_FAULT     2
#define PERF_IRQ            3
#define PERF_EVENT_MAX      8
#define MAX_PERF_SAMPLES    128

struct perf_sample {
    int event_type;
    int pid;
    uint64 timestamp;
    uint64 data;
};

static struct {
    struct spinlock lock;
    int enabled;
    struct perf_sample samples[MAX_PERF_SAMPLES];
    int head;
    int count;
    uint64 event_counts[PERF_EVENT_MAX];
    uint64 total_switches;
    uint64 total_syscalls;
    uint64 max_latency;
} profiler;

void kprofiler_init(void) {
    initlock(&profiler.lock, "kprofiler");
    profiler.enabled = 0;
    profiler.head = 0;
    profiler.count = 0;
    memset(profiler.event_counts, 0, sizeof(profiler.event_counts));
    printf("kprofiler: kernel profiler initialized\n");
}

void perf_record(int event, int pid, uint64 data) {
    if (!profiler.enabled) return;
    acquire(&profiler.lock);
    profiler.event_counts[event % PERF_EVENT_MAX]++;
    if (profiler.count < MAX_PERF_SAMPLES) {
        struct perf_sample *s = &profiler.samples[profiler.head];
        s->event_type = event;
        s->pid = pid;
        s->timestamp = ticks;
        s->data = data;
        profiler.head = (profiler.head + 1) % MAX_PERF_SAMPLES;
        profiler.count++;
    }
    release(&profiler.lock);
}

void perf_sched_switch(int from_pid, int to_pid) {
    perf_record(PERF_SCHED_SWITCH, to_pid, from_pid);
    acquire(&profiler.lock);
    profiler.total_switches++;
    release(&profiler.lock);
}

void perf_syscall(int pid, int syscall_num) {
    perf_record(PERF_SYSCALL, pid, syscall_num);
    acquire(&profiler.lock);
    profiler.total_syscalls++;
    release(&profiler.lock);
}

int kprofiler_enable(int enable) {
    acquire(&profiler.lock);
    profiler.enabled = enable;
    if (enable) {
        profiler.head = 0;
        profiler.count = 0;
        memset(profiler.event_counts, 0, sizeof(profiler.event_counts));
    }
    release(&profiler.lock);
    return 0;
}

void kprofiler_print_stats(void) {
    acquire(&profiler.lock);
    printf("\n=== Kernel Profiler Stats ===\n");
    printf("Enabled: %d\n", profiler.enabled);
    printf("Samples: %d\n", profiler.count);
    printf("Context switches: %d\n", (int)profiler.total_switches);
    printf("Syscalls: %d\n", (int)profiler.total_syscalls);
    printf("=============================\n");
    release(&profiler.lock);
}

int kprofiler_get_stats(char *buf, int len) {
    acquire(&profiler.lock);
    int off = snprintf(buf, len, "Profiler: switches=%d syscalls=%d samples=%d\n",
        (int)profiler.total_switches, (int)profiler.total_syscalls, profiler.count);
    release(&profiler.lock);
    return off;
}
