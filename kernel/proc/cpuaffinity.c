// cpuaffinity.c - CPU亲和性调度 (CPU Affinity)
// 实现SMP多核调度优化，支持进程绑定到特定CPU核心

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);

extern struct proc proc[];
extern uint ticks;

// ============ 常量定义 ============

#define MAX_CPUS 8              // 最大支持CPU数
#define CPU_MASK_ALL 0xFF       // 所有CPU掩码

// CPU负载均衡策略
#define LB_NONE         0       // 不进行负载均衡
#define LB_PERIODIC     1       // 周期性负载均衡
#define LB_ON_IDLE      2       // 空闲时负载均衡
#define LB_AGGRESSIVE   3       // 激进负载均衡

// ============ 数据结构 ============

// 每CPU统计信息
struct cpu_stats {
    uint64 total_ticks;         // 总时钟周期
    uint64 idle_ticks;          // 空闲周期
    uint64 user_ticks;          // 用户态周期
    uint64 kernel_ticks;        // 内核态周期
    uint64 context_switches;    // 上下文切换次数
    uint64 migrations_in;       // 迁入进程数
    uint64 migrations_out;      // 迁出进程数
    int runqueue_len;           // 运行队列长度
    int current_pid;            // 当前运行进程
};

// CPU亲和性管理器
struct affinity_manager {
    struct spinlock lock;
    int num_cpus;               // CPU数量
    int lb_policy;              // 负载均衡策略
    int lb_interval;            // 负载均衡间隔
    uint64 last_lb_tick;        // 上次负载均衡时间
    struct cpu_stats stats[MAX_CPUS];
    
    // 全局统计
    uint64 total_migrations;
    uint64 total_lb_runs;
    uint64 affinity_violations; // 亲和性违反次数
};

static struct affinity_manager aff_mgr;

// ============ 初始化 ============

void
cpuaffinity_init(void)
{
    initlock(&aff_mgr.lock, "cpuaffinity");
    
    // 检测CPU数量 (xv6默认使用NCPU)
    aff_mgr.num_cpus = NCPU;
    if (aff_mgr.num_cpus > MAX_CPUS) {
        aff_mgr.num_cpus = MAX_CPUS;
    }
    
    aff_mgr.lb_policy = LB_PERIODIC;
    aff_mgr.lb_interval = 100;  // 每100个tick进行一次负载均衡
    aff_mgr.last_lb_tick = 0;
    aff_mgr.total_migrations = 0;
    aff_mgr.total_lb_runs = 0;
    aff_mgr.affinity_violations = 0;
    
    // 初始化每CPU统计
    for (int i = 0; i < MAX_CPUS; i++) {
        memset(&aff_mgr.stats[i], 0, sizeof(struct cpu_stats));
        aff_mgr.stats[i].current_pid = -1;
    }
    
    printf("cpuaffinity: SMP scheduler initialized (%d CPUs)\n", aff_mgr.num_cpus);
}

// ============ 亲和性设置 ============

// 设置进程的CPU亲和性掩码
int
sched_setaffinity(int pid, uint32 mask)
{
    if (mask == 0) return -1;  // 至少要绑定一个CPU
    
    // 限制掩码范围
    uint32 valid_mask = (1 << aff_mgr.num_cpus) - 1;
    mask &= valid_mask;
    if (mask == 0) return -1;
    
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            p->cpu_affinity = mask;
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 获取进程的CPU亲和性掩码
int
sched_getaffinity(int pid, uint32 *mask)
{
    if (!mask) return -1;
    
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            *mask = p->cpu_affinity;
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 检查进程是否可以在指定CPU上运行
int
cpu_allowed(struct proc *p, int cpu)
{
    if (cpu < 0 || cpu >= aff_mgr.num_cpus) return 0;
    return (p->cpu_affinity & (1 << cpu)) != 0;
}

// ============ 负载均衡 ============

// 获取CPU负载
static int
get_cpu_load(int cpu)
{
    if (cpu < 0 || cpu >= aff_mgr.num_cpus) return 0;
    return aff_mgr.stats[cpu].runqueue_len;
}

// 找到负载最轻的CPU
int
find_least_loaded_cpu(uint32 mask)
{
    int best_cpu = -1;
    int min_load = 0x7FFFFFFF;
    
    for (int i = 0; i < aff_mgr.num_cpus; i++) {
        if (mask & (1 << i)) {
            int load = get_cpu_load(i);
            if (load < min_load) {
                min_load = load;
                best_cpu = i;
            }
        }
    }
    
    return best_cpu;
}

// 找到负载最重的CPU
static int
find_most_loaded_cpu(void)
{
    int worst_cpu = -1;
    int max_load = -1;
    
    for (int i = 0; i < aff_mgr.num_cpus; i++) {
        int load = get_cpu_load(i);
        if (load > max_load) {
            max_load = load;
            worst_cpu = i;
        }
    }
    
    return worst_cpu;
}

// 尝试迁移进程
static int
try_migrate_process(struct proc *p, int from_cpu, int to_cpu)
{
    if (!cpu_allowed(p, to_cpu)) {
        aff_mgr.affinity_violations++;
        return -1;
    }
    
    // 更新统计
    aff_mgr.stats[from_cpu].migrations_out++;
    aff_mgr.stats[to_cpu].migrations_in++;
    aff_mgr.total_migrations++;
    
    // 实际迁移由调度器在下次调度时完成
    // 这里只是标记进程应该迁移
    p->preferred_cpu = to_cpu;
    
    return 0;
}

// 执行负载均衡
void
load_balance(void)
{
    if (aff_mgr.lb_policy == LB_NONE) return;
    
    acquire(&aff_mgr.lock);
    
    // 检查是否需要负载均衡
    if (aff_mgr.lb_policy == LB_PERIODIC) {
        if (ticks - aff_mgr.last_lb_tick < aff_mgr.lb_interval) {
            release(&aff_mgr.lock);
            return;
        }
    }
    
    aff_mgr.last_lb_tick = ticks;
    aff_mgr.total_lb_runs++;
    
    // 计算平均负载
    int total_load = 0;
    for (int i = 0; i < aff_mgr.num_cpus; i++) {
        total_load += get_cpu_load(i);
    }
    (void)total_load;  // 用于后续扩展
    
    // 找到负载不均衡的CPU
    int busiest = find_most_loaded_cpu();
    int idlest = find_least_loaded_cpu(CPU_MASK_ALL);
    
    if (busiest < 0 || idlest < 0 || busiest == idlest) {
        release(&aff_mgr.lock);
        return;
    }
    
    int load_diff = get_cpu_load(busiest) - get_cpu_load(idlest);
    
    // 如果负载差异足够大，尝试迁移进程
    if (load_diff > 2) {
        // 从最忙的CPU找一个可迁移的进程
        struct proc *p;
        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE && cpu_allowed(p, idlest)) {
                // 找到一个可迁移的进程
                try_migrate_process(p, busiest, idlest);
                release(&p->lock);
                break;
            }
            release(&p->lock);
        }
    }
    
    release(&aff_mgr.lock);
}

// ============ 统计更新 ============

// 更新CPU统计 (每个时钟中断调用)
void
cpu_stats_tick(int cpu, int is_idle)
{
    if (cpu < 0 || cpu >= aff_mgr.num_cpus) return;
    
    acquire(&aff_mgr.lock);
    
    aff_mgr.stats[cpu].total_ticks++;
    if (is_idle) {
        aff_mgr.stats[cpu].idle_ticks++;
    }
    
    release(&aff_mgr.lock);
}

// 记录上下文切换
void
cpu_stats_switch(int cpu, int new_pid)
{
    if (cpu < 0 || cpu >= aff_mgr.num_cpus) return;
    
    acquire(&aff_mgr.lock);
    
    aff_mgr.stats[cpu].context_switches++;
    aff_mgr.stats[cpu].current_pid = new_pid;
    
    release(&aff_mgr.lock);
}

// 更新运行队列长度
void
cpu_stats_runqueue(int cpu, int len)
{
    if (cpu < 0 || cpu >= aff_mgr.num_cpus) return;
    
    acquire(&aff_mgr.lock);
    aff_mgr.stats[cpu].runqueue_len = len;
    release(&aff_mgr.lock);
}

// ============ 配置接口 ============

// 设置负载均衡策略
int
lb_set_policy(int policy)
{
    if (policy < LB_NONE || policy > LB_AGGRESSIVE) return -1;
    
    acquire(&aff_mgr.lock);
    aff_mgr.lb_policy = policy;
    release(&aff_mgr.lock);
    
    return 0;
}

// 设置负载均衡间隔
int
lb_set_interval(int interval)
{
    if (interval < 1) return -1;
    
    acquire(&aff_mgr.lock);
    aff_mgr.lb_interval = interval;
    release(&aff_mgr.lock);
    
    return 0;
}

// ============ 统计输出 ============

void
cpuaffinity_print_stats(void)
{
    acquire(&aff_mgr.lock);
    
    printf("\n=== CPU Affinity Statistics ===\n");
    printf("CPUs: %d\n", aff_mgr.num_cpus);
    printf("LB Policy: %d, Interval: %d\n", aff_mgr.lb_policy, aff_mgr.lb_interval);
    printf("Total migrations: %d\n", (int)aff_mgr.total_migrations);
    printf("Total LB runs: %d\n", (int)aff_mgr.total_lb_runs);
    printf("Affinity violations: %d\n", (int)aff_mgr.affinity_violations);
    
    printf("\nPer-CPU stats:\n");
    for (int i = 0; i < aff_mgr.num_cpus; i++) {
        struct cpu_stats *s = &aff_mgr.stats[i];
        printf("  CPU%d: ticks=%d idle=%d switches=%d runq=%d\n",
               i, (int)s->total_ticks, (int)s->idle_ticks,
               (int)s->context_switches, s->runqueue_len);
    }
    printf("================================\n");
    
    release(&aff_mgr.lock);
}

int
cpuaffinity_get_info(char *buf, int len)
{
    acquire(&aff_mgr.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "CPU Affinity Info:\n");
    offset += snprintf(buf + offset, len - offset,
        "  CPUs: %d\n", aff_mgr.num_cpus);
    offset += snprintf(buf + offset, len - offset,
        "  Migrations: %d\n", (int)aff_mgr.total_migrations);
    
    for (int i = 0; i < aff_mgr.num_cpus && offset < len - 64; i++) {
        struct cpu_stats *s = &aff_mgr.stats[i];
        int util = s->total_ticks > 0 ? 
            100 - (s->idle_ticks * 100 / s->total_ticks) : 0;
        offset += snprintf(buf + offset, len - offset,
            "  CPU%d: %d%% util, %d switches\n",
            i, util, (int)s->context_switches);
    }
    
    release(&aff_mgr.lock);
    return offset;
}
