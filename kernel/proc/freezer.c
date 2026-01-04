// freezer.c - 进程冻结/解冻 (Process Freezer)
// 类似Android的进程管理，支持冻结后台进程以节省资源

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

// 冻结状态
#define FREEZE_STATE_THAWED     0   // 解冻状态
#define FREEZE_STATE_FREEZING   1   // 正在冻结
#define FREEZE_STATE_FROZEN     2   // 已冻结

// 冻结原因
#define FREEZE_REASON_USER      1   // 用户请求
#define FREEZE_REASON_SYSTEM    2   // 系统请求
#define FREEZE_REASON_LOWMEM    3   // 内存不足
#define FREEZE_REASON_SUSPEND   4   // 系统挂起
#define FREEZE_REASON_CGROUP    5   // cgroup冻结

// 冻结优先级 (数字越大越容易被冻结)
#define FREEZE_PRIO_CRITICAL    0   // 关键进程，不冻结
#define FREEZE_PRIO_FOREGROUND  1   // 前台进程
#define FREEZE_PRIO_VISIBLE     2   // 可见进程
#define FREEZE_PRIO_SERVICE     3   // 服务进程
#define FREEZE_PRIO_BACKGROUND  4   // 后台进程
#define FREEZE_PRIO_CACHED      5   // 缓存进程

#define MAX_FROZEN_PROCS 32

// ============ 数据结构 ============

// 冻结进程记录
struct frozen_proc {
    int used;
    int pid;
    int freeze_state;
    int freeze_reason;
    int freeze_prio;
    uint64 freeze_time;
    uint64 thaw_time;
    uint64 frozen_duration;     // 累计冻结时间
};

// 冻结管理器
struct freezer_manager {
    struct spinlock lock;
    struct frozen_proc frozen[MAX_FROZEN_PROCS];
    int frozen_count;
    
    // 自动冻结配置
    int auto_freeze_enabled;
    int auto_freeze_threshold;  // 内存阈值
    int freeze_timeout;         // 后台进程冻结超时
    
    // 统计
    uint64 total_freezes;
    uint64 total_thaws;
    uint64 total_frozen_time;
    uint64 oom_freezes;         // OOM触发的冻结
};

static struct freezer_manager freezer;

// ============ 初始化 ============

void
freezer_init(void)
{
    initlock(&freezer.lock, "freezer");
    
    memset(freezer.frozen, 0, sizeof(freezer.frozen));
    freezer.frozen_count = 0;
    freezer.auto_freeze_enabled = 1;
    freezer.auto_freeze_threshold = 80;  // 80%内存使用时开始冻结
    freezer.freeze_timeout = 3000;       // 30秒后台超时
    freezer.total_freezes = 0;
    freezer.total_thaws = 0;
    freezer.total_frozen_time = 0;
    freezer.oom_freezes = 0;
    
    printf("freezer: process freezer initialized\n");
}

// ============ 辅助函数 ============

// 查找冻结记录
static struct frozen_proc*
find_frozen_record(int pid)
{
    for (int i = 0; i < MAX_FROZEN_PROCS; i++) {
        if (freezer.frozen[i].used && freezer.frozen[i].pid == pid) {
            return &freezer.frozen[i];
        }
    }
    return 0;
}

// 分配冻结记录
static struct frozen_proc*
alloc_frozen_record(void)
{
    for (int i = 0; i < MAX_FROZEN_PROCS; i++) {
        if (!freezer.frozen[i].used) {
            return &freezer.frozen[i];
        }
    }
    return 0;
}

// ============ 冻结操作 ============

// 冻结进程
int
freeze_process(int pid, int reason)
{
    struct proc *p;
    
    // 查找进程
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            // 检查是否可以冻结
            if (p->frozen) {
                release(&p->lock);
                return 0;  // 已经冻结
            }
            
            // 不能冻结init进程
            if (pid == 1) {
                release(&p->lock);
                return -1;
            }
            
            // 设置冻结状态
            p->frozen = 1;
            p->freeze_reason = reason;
            
            // 如果进程正在运行，标记为需要冻结
            // 实际冻结在下次调度时生效
            
            release(&p->lock);
            
            // 记录冻结信息
            acquire(&freezer.lock);
            
            struct frozen_proc *fp = find_frozen_record(pid);
            if (!fp) {
                fp = alloc_frozen_record();
            }
            
            if (fp) {
                fp->used = 1;
                fp->pid = pid;
                fp->freeze_state = FREEZE_STATE_FROZEN;
                fp->freeze_reason = reason;
                fp->freeze_time = ticks;
                freezer.frozen_count++;
            }
            
            freezer.total_freezes++;
            if (reason == FREEZE_REASON_LOWMEM) {
                freezer.oom_freezes++;
            }
            
            release(&freezer.lock);
            
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 解冻进程
int
thaw_process(int pid)
{
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            if (!p->frozen) {
                release(&p->lock);
                return 0;  // 未冻结
            }
            
            // 解冻
            p->frozen = 0;
            p->freeze_reason = 0;
            
            // 如果进程之前是RUNNABLE，恢复调度
            if (p->state == SLEEPING) {
                // 可能需要唤醒
            }
            
            release(&p->lock);
            
            // 更新记录
            acquire(&freezer.lock);
            
            struct frozen_proc *fp = find_frozen_record(pid);
            if (fp) {
                fp->thaw_time = ticks;
                fp->frozen_duration += ticks - fp->freeze_time;
                fp->freeze_state = FREEZE_STATE_THAWED;
                freezer.total_frozen_time += ticks - fp->freeze_time;
                fp->used = 0;
                freezer.frozen_count--;
            }
            
            freezer.total_thaws++;
            
            release(&freezer.lock);
            
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 冻结cgroup中的所有进程
int
freeze_cgroup(int cgroup_id)
{
    int frozen = 0;
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != UNUSED && p->cgroup_id == cgroup_id) {
            int pid = p->pid;
            release(&p->lock);
            
            if (freeze_process(pid, FREEZE_REASON_CGROUP) == 0) {
                frozen++;
            }
        } else {
            release(&p->lock);
        }
    }
    
    return frozen;
}

// 解冻cgroup中的所有进程
int
thaw_cgroup(int cgroup_id)
{
    int thawed = 0;
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != UNUSED && p->cgroup_id == cgroup_id && p->frozen) {
            int pid = p->pid;
            release(&p->lock);
            
            if (thaw_process(pid) == 0) {
                thawed++;
            }
        } else {
            release(&p->lock);
        }
    }
    
    return thawed;
}

// ============ 自动冻结 ============

// 设置进程冻结优先级
int
freeze_set_priority(int pid, int prio)
{
    if (prio < FREEZE_PRIO_CRITICAL || prio > FREEZE_PRIO_CACHED) {
        return -1;
    }
    
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            p->freeze_prio = prio;
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 自动冻结后台进程 (低内存时调用)
int
auto_freeze_background(void)
{
    if (!freezer.auto_freeze_enabled) return 0;
    
    int frozen = 0;
    struct proc *p;
    
    // 按优先级从高到低冻结
    for (int prio = FREEZE_PRIO_CACHED; prio >= FREEZE_PRIO_BACKGROUND; prio--) {
        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state != UNUSED && !p->frozen && p->freeze_prio >= prio) {
                int pid = p->pid;
                release(&p->lock);
                
                if (freeze_process(pid, FREEZE_REASON_LOWMEM) == 0) {
                    frozen++;
                }
            } else {
                release(&p->lock);
            }
        }
    }
    
    return frozen;
}

// ============ 查询接口 ============

// 检查进程是否被冻结
int
is_frozen(int pid)
{
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            int frozen = p->frozen;
            release(&p->lock);
            return frozen;
        }
        release(&p->lock);
    }
    
    return 0;
}

// 获取冻结进程数量
int
frozen_count(void)
{
    acquire(&freezer.lock);
    int count = freezer.frozen_count;
    release(&freezer.lock);
    return count;
}

// ============ 配置接口 ============

int
freezer_set_auto(int enabled)
{
    acquire(&freezer.lock);
    freezer.auto_freeze_enabled = enabled;
    release(&freezer.lock);
    return 0;
}

int
freezer_set_timeout(int timeout)
{
    if (timeout < 0) return -1;
    acquire(&freezer.lock);
    freezer.freeze_timeout = timeout;
    release(&freezer.lock);
    return 0;
}

// ============ 统计输出 ============

void
freezer_print_stats(void)
{
    acquire(&freezer.lock);
    
    printf("\n=== Freezer Statistics ===\n");
    printf("Currently frozen: %d\n", freezer.frozen_count);
    printf("Total freezes: %d\n", (int)freezer.total_freezes);
    printf("Total thaws: %d\n", (int)freezer.total_thaws);
    printf("OOM freezes: %d\n", (int)freezer.oom_freezes);
    printf("Total frozen time: %d ticks\n", (int)freezer.total_frozen_time);
    printf("Auto-freeze: %s\n", freezer.auto_freeze_enabled ? "enabled" : "disabled");
    printf("==========================\n");
    
    release(&freezer.lock);
}

int
freezer_list(char *buf, int len)
{
    acquire(&freezer.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "Frozen processes:\n");
    offset += snprintf(buf + offset, len - offset,
        "PID\tSTATE\t\tREASON\tTIME\n");
    
    char *reasons[] = {"", "USER", "SYSTEM", "LOWMEM", "SUSPEND", "CGROUP"};
    
    for (int i = 0; i < MAX_FROZEN_PROCS && offset < len - 64; i++) {
        struct frozen_proc *fp = &freezer.frozen[i];
        if (fp->used) {
            offset += snprintf(buf + offset, len - offset,
                "%d\tFROZEN\t\t%s\t%d\n",
                fp->pid, reasons[fp->freeze_reason], (int)fp->freeze_time);
        }
    }
    
    release(&freezer.lock);
    return offset;
}
