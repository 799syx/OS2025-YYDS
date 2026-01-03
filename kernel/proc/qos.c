// QoS (Quality of Service) 调度器
// 参考鸿蒙 QoS 设计，实现基于服务质量的任务调度

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

// 使用内核的 snprintf
int snprintf(char *buf, int size, const char *fmt, ...);

// ============ QoS 级别定义 ============
// 从高到低的优先级

#define QOS_USER_INTERACTIVE  0   // 用户交互（最高优先级）- UI响应
#define QOS_DEADLINE_REQUEST  1   // 截止时间请求 - 有时间限制的任务
#define QOS_USER_INITIATED    2   // 用户发起 - 用户主动触发的任务
#define QOS_DEFAULT           3   // 默认级别
#define QOS_UTILITY           4   // 实用工具 - 后台计算任务
#define QOS_BACKGROUND        5   // 后台任务（最低优先级）
#define QOS_LEVELS            6

// QoS 级别名称
static char *qos_names[QOS_LEVELS] = {
    "USER_INTERACTIVE",
    "DEADLINE_REQUEST", 
    "USER_INITIATED",
    "DEFAULT",
    "UTILITY",
    "BACKGROUND"
};

// 每个 QoS 级别的时间片（ticks）
static int qos_timeslice[QOS_LEVELS] = {
    2,    // USER_INTERACTIVE: 短时间片，快速响应
    4,    // DEADLINE_REQUEST: 较短时间片
    8,    // USER_INITIATED: 中等时间片
    16,   // DEFAULT: 标准时间片
    32,   // UTILITY: 较长时间片
    64    // BACKGROUND: 最长时间片
};

// 每个 QoS 级别的优先级提升阈值（等待多少 ticks 后提升）
static int qos_boost_threshold[QOS_LEVELS] = {
    0,     // USER_INTERACTIVE: 不需要提升
    50,    // DEADLINE_REQUEST
    100,   // USER_INITIATED
    200,   // DEFAULT
    400,   // UTILITY
    800    // BACKGROUND
};

// ============ QoS 任务结构 ============

struct qos_task {
    struct proc *p;           // 关联的进程
    int qos_level;            // QoS 级别
    int base_qos;             // 基础 QoS（用于恢复）
    int remaining_slice;      // 剩余时间片
    int wait_ticks;           // 等待时间
    int cpu_usage;            // CPU 使用率（百分比）
    int io_wait;              // I/O 等待时间
    uint64 deadline;          // 截止时间（如果有）
    int boosted;              // 是否被提升过
    struct qos_task *next;    // 链表
};

// QoS 就绪队列（每个级别一个）
struct qos_queue {
    struct spinlock lock;
    struct qos_task *head;
    struct qos_task *tail;
    int count;
};

static struct qos_queue qos_queues[QOS_LEVELS];

// QoS 任务池
#define MAX_QOS_TASKS 128
static struct qos_task qos_task_pool[MAX_QOS_TASKS];
static struct spinlock qos_pool_lock;

// QoS 统计
struct qos_stats {
    uint64 total_schedules;
    uint64 level_schedules[QOS_LEVELS];
    uint64 priority_boosts;
    uint64 deadline_misses;
    uint64 context_switches;
};
static struct qos_stats qos_stats;
static struct spinlock qos_stats_lock;

// QoS 全局配置
struct qos_config {
    int enabled;              // 是否启用 QoS
    int auto_boost;           // 自动优先级提升
    int deadline_aware;       // 截止时间感知
    int io_boost;             // I/O 密集型任务提升
};
static struct qos_config qos_config = {
    .enabled = 1,
    .auto_boost = 1,
    .deadline_aware = 1,
    .io_boost = 1
};

// ============ QoS 初始化 ============

void
qos_init(void)
{
    initlock(&qos_pool_lock, "qos_pool");
    initlock(&qos_stats_lock, "qos_stats");
    
    memset(qos_task_pool, 0, sizeof(qos_task_pool));
    memset(&qos_stats, 0, sizeof(qos_stats));
    
    for (int i = 0; i < QOS_LEVELS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "qos_q%d", i);
        initlock(&qos_queues[i].lock, name);
        qos_queues[i].head = 0;
        qos_queues[i].tail = 0;
        qos_queues[i].count = 0;
    }
    
    printf("qos: initialized with %d levels\n", QOS_LEVELS);
}

// ============ QoS 任务管理 ============

// 分配 QoS 任务结构
static struct qos_task*
qos_task_alloc(void)
{
    acquire(&qos_pool_lock);
    
    for (int i = 0; i < MAX_QOS_TASKS; i++) {
        if (qos_task_pool[i].p == 0) {
            memset(&qos_task_pool[i], 0, sizeof(struct qos_task));
            release(&qos_pool_lock);
            return &qos_task_pool[i];
        }
    }
    
    release(&qos_pool_lock);
    return 0;
}

// 释放 QoS 任务结构
static void
qos_task_free(struct qos_task *task)
{
    if (task == 0)
        return;
    
    acquire(&qos_pool_lock);
    task->p = 0;
    release(&qos_pool_lock);
}

// 查找进程对应的 QoS 任务
static struct qos_task*
qos_find_task(struct proc *p)
{
    for (int i = 0; i < MAX_QOS_TASKS; i++) {
        if (qos_task_pool[i].p == p) {
            return &qos_task_pool[i];
        }
    }
    return 0;
}

// 将任务加入队列
static void
qos_enqueue(struct qos_task *task)
{
    int level = task->qos_level;
    if (level < 0 || level >= QOS_LEVELS)
        level = QOS_DEFAULT;
    
    acquire(&qos_queues[level].lock);
    
    task->next = 0;
    if (qos_queues[level].tail) {
        qos_queues[level].tail->next = task;
    } else {
        qos_queues[level].head = task;
    }
    qos_queues[level].tail = task;
    qos_queues[level].count++;
    
    release(&qos_queues[level].lock);
}

// 从队列移除任务
static struct qos_task*
qos_dequeue(int level)
{
    if (level < 0 || level >= QOS_LEVELS)
        return 0;
    
    acquire(&qos_queues[level].lock);
    
    struct qos_task *task = qos_queues[level].head;
    if (task) {
        qos_queues[level].head = task->next;
        if (qos_queues[level].head == 0) {
            qos_queues[level].tail = 0;
        }
        qos_queues[level].count--;
        task->next = 0;
    }
    
    release(&qos_queues[level].lock);
    return task;
}

// ============ QoS 核心调度 ============

// 注册进程到 QoS 调度器
int
qos_register(struct proc *p, int qos_level)
{
    if (!qos_config.enabled)
        return -1;
    
    if (qos_level < 0 || qos_level >= QOS_LEVELS)
        qos_level = QOS_DEFAULT;
    
    struct qos_task *task = qos_find_task(p);
    if (task == 0) {
        task = qos_task_alloc();
        if (task == 0)
            return -1;
    }
    
    task->p = p;
    task->qos_level = qos_level;
    task->base_qos = qos_level;
    task->remaining_slice = qos_timeslice[qos_level];
    task->wait_ticks = 0;
    task->cpu_usage = 0;
    task->io_wait = 0;
    task->deadline = 0;
    task->boosted = 0;
    
    return 0;
}

// 注销进程
void
qos_unregister(struct proc *p)
{
    struct qos_task *task = qos_find_task(p);
    if (task) {
        qos_task_free(task);
    }
}

// 设置进程的 QoS 级别
int
qos_set_level(struct proc *p, int qos_level)
{
    if (qos_level < 0 || qos_level >= QOS_LEVELS)
        return -1;
    
    struct qos_task *task = qos_find_task(p);
    if (task == 0) {
        // 自动注册
        return qos_register(p, qos_level);
    }
    
    task->qos_level = qos_level;
    task->base_qos = qos_level;
    task->remaining_slice = qos_timeslice[qos_level];
    task->boosted = 0;
    
    return 0;
}

// 获取进程的 QoS 级别
int
qos_get_level(struct proc *p)
{
    struct qos_task *task = qos_find_task(p);
    if (task == 0)
        return QOS_DEFAULT;
    return task->qos_level;
}

// 设置截止时间
int
qos_set_deadline(struct proc *p, uint64 deadline)
{
    struct qos_task *task = qos_find_task(p);
    if (task == 0)
        return -1;
    
    task->deadline = deadline;
    
    // 有截止时间的任务自动提升到 DEADLINE_REQUEST
    if (deadline > 0 && task->qos_level > QOS_DEADLINE_REQUEST) {
        task->qos_level = QOS_DEADLINE_REQUEST;
    }
    
    return 0;
}

// 优先级提升检查
static void
qos_check_boost(struct qos_task *task)
{
    if (!qos_config.auto_boost)
        return;
    
    if (task->qos_level <= QOS_USER_INTERACTIVE)
        return;  // 已经是最高级别
    
    int threshold = qos_boost_threshold[task->qos_level];
    if (task->wait_ticks >= threshold) {
        // 提升优先级
        task->qos_level--;
        task->remaining_slice = qos_timeslice[task->qos_level];
        task->wait_ticks = 0;
        task->boosted = 1;
        
        acquire(&qos_stats_lock);
        qos_stats.priority_boosts++;
        release(&qos_stats_lock);
    }
}

// 优先级恢复
static void
qos_restore_priority(struct qos_task *task)
{
    if (task->boosted && task->qos_level < task->base_qos) {
        task->qos_level = task->base_qos;
        task->remaining_slice = qos_timeslice[task->qos_level];
        task->boosted = 0;
    }
}

// QoS 调度决策 - 选择下一个要运行的进程
struct proc*
qos_schedule(void)
{
    if (!qos_config.enabled)
        return 0;
    
    acquire(&qos_stats_lock);
    qos_stats.total_schedules++;
    release(&qos_stats_lock);
    
    // 按优先级从高到低遍历队列
    for (int level = 0; level < QOS_LEVELS; level++) {
        struct qos_task *task = qos_dequeue(level);
        if (task && task->p && task->p->state == RUNNABLE) {
            acquire(&qos_stats_lock);
            qos_stats.level_schedules[level]++;
            qos_stats.context_switches++;
            release(&qos_stats_lock);
            
            // 重置时间片
            task->remaining_slice = qos_timeslice[level];
            task->wait_ticks = 0;
            
            // 恢复优先级（如果被提升过）
            qos_restore_priority(task);
            
            return task->p;
        }
    }
    
    return 0;
}

// 时钟 tick 处理
void
qos_tick(struct proc *p)
{
    if (!qos_config.enabled)
        return;
    
    struct qos_task *task = qos_find_task(p);
    if (task == 0)
        return;
    
    // 减少剩余时间片
    if (task->remaining_slice > 0) {
        task->remaining_slice--;
    }
    
    // 更新 CPU 使用率
    task->cpu_usage++;
    
    // 检查截止时间
    if (qos_config.deadline_aware && task->deadline > 0) {
        extern uint ticks;
        if (ticks >= task->deadline) {
            acquire(&qos_stats_lock);
            qos_stats.deadline_misses++;
            release(&qos_stats_lock);
            task->deadline = 0;  // 清除截止时间
        }
    }
    
    // 更新等待队列中任务的等待时间
    for (int level = 0; level < QOS_LEVELS; level++) {
        acquire(&qos_queues[level].lock);
        for (struct qos_task *t = qos_queues[level].head; t; t = t->next) {
            t->wait_ticks++;
            qos_check_boost(t);
        }
        release(&qos_queues[level].lock);
    }
}

// 检查是否需要抢占
int
qos_should_preempt(struct proc *current)
{
    if (!qos_config.enabled)
        return 0;
    
    struct qos_task *task = qos_find_task(current);
    if (task == 0)
        return 0;
    
    // 时间片用完
    if (task->remaining_slice <= 0)
        return 1;
    
    // 检查是否有更高优先级的任务
    for (int level = 0; level < task->qos_level; level++) {
        if (qos_queues[level].count > 0)
            return 1;
    }
    
    return 0;
}

// 进程让出 CPU
void
qos_yield(struct proc *p)
{
    struct qos_task *task = qos_find_task(p);
    if (task) {
        qos_enqueue(task);
    }
}

// I/O 完成通知（用于 I/O 密集型任务提升）
void
qos_io_complete(struct proc *p)
{
    if (!qos_config.io_boost)
        return;
    
    struct qos_task *task = qos_find_task(p);
    if (task && task->qos_level > QOS_USER_INITIATED) {
        // I/O 密集型任务临时提升
        task->qos_level = QOS_USER_INITIATED;
        task->boosted = 1;
    }
}

// ============ QoS 统计和调试 ============

// 获取 QoS 统计信息
void
qos_get_stats(uint64 *total, uint64 *boosts, uint64 *misses, uint64 *switches)
{
    acquire(&qos_stats_lock);
    if (total) *total = qos_stats.total_schedules;
    if (boosts) *boosts = qos_stats.priority_boosts;
    if (misses) *misses = qos_stats.deadline_misses;
    if (switches) *switches = qos_stats.context_switches;
    release(&qos_stats_lock);
}

// 打印 QoS 统计信息
void
qos_print_stats(void)
{
    printf("\n=== QoS Scheduler Statistics ===\n");
    printf("Total schedules: %d\n", (int)qos_stats.total_schedules);
    printf("Context switches: %d\n", (int)qos_stats.context_switches);
    printf("Priority boosts: %d\n", (int)qos_stats.priority_boosts);
    printf("Deadline misses: %d\n", (int)qos_stats.deadline_misses);
    
    printf("\nPer-level statistics:\n");
    for (int i = 0; i < QOS_LEVELS; i++) {
        printf("  [%d] %-18s: schedules=%d queue=%d\n",
               i, qos_names[i], 
               (int)qos_stats.level_schedules[i],
               qos_queues[i].count);
    }
    
    printf("\nConfiguration:\n");
    printf("  Enabled: %s\n", qos_config.enabled ? "yes" : "no");
    printf("  Auto boost: %s\n", qos_config.auto_boost ? "yes" : "no");
    printf("  Deadline aware: %s\n", qos_config.deadline_aware ? "yes" : "no");
    printf("  I/O boost: %s\n", qos_config.io_boost ? "yes" : "no");
    printf("=================================\n");
}

// 打印进程的 QoS 信息
void
qos_print_task(struct proc *p)
{
    struct qos_task *task = qos_find_task(p);
    if (task == 0) {
        printf("Process %d: not registered with QoS\n", p->pid);
        return;
    }
    
    printf("Process %d (%s):\n", p->pid, p->name);
    printf("  QoS level: %d (%s)\n", task->qos_level, qos_names[task->qos_level]);
    printf("  Base QoS: %d (%s)\n", task->base_qos, qos_names[task->base_qos]);
    printf("  Remaining slice: %d\n", task->remaining_slice);
    printf("  Wait ticks: %d\n", task->wait_ticks);
    printf("  CPU usage: %d\n", task->cpu_usage);
    printf("  Boosted: %s\n", task->boosted ? "yes" : "no");
    if (task->deadline > 0) {
        printf("  Deadline: %d\n", (int)task->deadline);
    }
}

// 配置 QoS
void
qos_configure(int enabled, int auto_boost, int deadline_aware, int io_boost)
{
    qos_config.enabled = enabled;
    qos_config.auto_boost = auto_boost;
    qos_config.deadline_aware = deadline_aware;
    qos_config.io_boost = io_boost;
}

// 获取 QoS 级别名称
char*
qos_level_name(int level)
{
    if (level < 0 || level >= QOS_LEVELS)
        return "UNKNOWN";
    return qos_names[level];
}
