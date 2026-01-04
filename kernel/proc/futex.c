// futex.c - 快速用户态互斥锁 (Fast Userspace Mutex)
// 实现高性能的用户态同步原语，减少系统调用开销

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

#define FUTEX_WAIT          0   // 等待futex值变化
#define FUTEX_WAKE          1   // 唤醒等待的进程
#define FUTEX_REQUEUE       2   // 重新排队
#define FUTEX_CMP_REQUEUE   3   // 条件重新排队
#define FUTEX_WAKE_OP       4   // 唤醒并操作
#define FUTEX_WAIT_BITSET   5   // 带位掩码等待
#define FUTEX_WAKE_BITSET   6   // 带位掩码唤醒

#define FUTEX_PRIVATE_FLAG  128 // 私有futex标志

#define MAX_FUTEX_QUEUES    64  // 最大futex队列数
#define FUTEX_HASH_SIZE     32  // 哈希表大小

// ============ 数据结构 ============

// Futex等待队列项
struct futex_waiter {
    int used;
    struct proc *proc;          // 等待的进程
    uint64 uaddr;               // 用户空间地址
    uint32 val;                 // 期望值
    uint32 bitset;              // 位掩码
    uint64 timeout;             // 超时时间
    int woken;                  // 是否被唤醒
};

// Futex哈希桶
struct futex_bucket {
    struct spinlock lock;
    struct futex_waiter waiters[MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE];
    int count;
};

// Futex管理器
struct futex_manager {
    struct spinlock lock;
    struct futex_bucket buckets[FUTEX_HASH_SIZE];
    
    // 统计
    uint64 total_waits;
    uint64 total_wakes;
    uint64 total_timeouts;
    uint64 total_requeues;
    uint64 spurious_wakeups;
};

static struct futex_manager futex_mgr;

// ============ 初始化 ============

void
futex_init(void)
{
    initlock(&futex_mgr.lock, "futex");
    
    for (int i = 0; i < FUTEX_HASH_SIZE; i++) {
        initlock(&futex_mgr.buckets[i].lock, "futex_bucket");
        futex_mgr.buckets[i].count = 0;
        for (int j = 0; j < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE; j++) {
            futex_mgr.buckets[i].waiters[j].used = 0;
        }
    }
    
    futex_mgr.total_waits = 0;
    futex_mgr.total_wakes = 0;
    futex_mgr.total_timeouts = 0;
    futex_mgr.total_requeues = 0;
    futex_mgr.spurious_wakeups = 0;
    
    printf("futex: fast userspace mutex initialized\n");
}

// ============ 辅助函数 ============

// 计算哈希值
static int
futex_hash(uint64 uaddr)
{
    return (int)((uaddr >> 2) % FUTEX_HASH_SIZE);
}

// 查找空闲等待槽
static struct futex_waiter*
find_free_waiter(struct futex_bucket *bucket)
{
    for (int i = 0; i < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE; i++) {
        if (!bucket->waiters[i].used) {
            return &bucket->waiters[i];
        }
    }
    return 0;
}

// 查找匹配的等待者
__attribute__((unused))
static struct futex_waiter*
find_waiter(struct futex_bucket *bucket, uint64 uaddr, uint32 bitset)
{
    for (int i = 0; i < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE; i++) {
        struct futex_waiter *w = &bucket->waiters[i];
        if (w->used && w->uaddr == uaddr && !w->woken) {
            if (bitset == 0 || (w->bitset & bitset)) {
                return w;
            }
        }
    }
    return 0;
}

// ============ Futex操作 ============

// FUTEX_WAIT: 等待futex值变化
int
futex_wait(uint64 uaddr, uint32 val, uint64 timeout)
{
    struct proc *p = myproc();
    int hash = futex_hash(uaddr);
    struct futex_bucket *bucket = &futex_mgr.buckets[hash];
    
    acquire(&bucket->lock);
    
    // 检查当前值是否与期望值匹配
    // 注意: 实际实现需要从用户空间读取值
    // 这里简化处理
    
    // 查找空闲等待槽
    struct futex_waiter *waiter = find_free_waiter(bucket);
    if (!waiter) {
        release(&bucket->lock);
        return -1;  // 队列已满
    }
    
    // 设置等待者信息
    waiter->used = 1;
    waiter->proc = p;
    waiter->uaddr = uaddr;
    waiter->val = val;
    waiter->bitset = 0xFFFFFFFF;
    waiter->timeout = timeout > 0 ? ticks + timeout : 0;
    waiter->woken = 0;
    bucket->count++;
    
    // 更新统计
    acquire(&futex_mgr.lock);
    futex_mgr.total_waits++;
    release(&futex_mgr.lock);
    
    release(&bucket->lock);
    
    // 进入睡眠
    acquire(&p->lock);
    p->state = SLEEPING;
    p->chan = (void*)uaddr;
    
    // 调度其他进程
    sched();
    
    // 被唤醒后
    p->chan = 0;
    release(&p->lock);
    
    // 清理等待者
    acquire(&bucket->lock);
    waiter->used = 0;
    bucket->count--;
    
    int result = waiter->woken ? 0 : -1;
    release(&bucket->lock);
    
    return result;
}

// FUTEX_WAKE: 唤醒等待的进程
int
futex_wake(uint64 uaddr, int nr_wake)
{
    int hash = futex_hash(uaddr);
    struct futex_bucket *bucket = &futex_mgr.buckets[hash];
    int woken = 0;
    
    acquire(&bucket->lock);
    
    for (int i = 0; i < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE && woken < nr_wake; i++) {
        struct futex_waiter *w = &bucket->waiters[i];
        if (w->used && w->uaddr == uaddr && !w->woken) {
            // 唤醒进程
            w->woken = 1;
            
            acquire(&w->proc->lock);
            if (w->proc->state == SLEEPING && w->proc->chan == (void*)uaddr) {
                w->proc->state = RUNNABLE;
            }
            release(&w->proc->lock);
            
            woken++;
        }
    }
    
    // 更新统计
    acquire(&futex_mgr.lock);
    futex_mgr.total_wakes += woken;
    release(&futex_mgr.lock);
    
    release(&bucket->lock);
    
    return woken;
}

// FUTEX_WAKE_BITSET: 带位掩码唤醒
int
futex_wake_bitset(uint64 uaddr, int nr_wake, uint32 bitset)
{
    if (bitset == 0) return -1;
    
    int hash = futex_hash(uaddr);
    struct futex_bucket *bucket = &futex_mgr.buckets[hash];
    int woken = 0;
    
    acquire(&bucket->lock);
    
    for (int i = 0; i < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE && woken < nr_wake; i++) {
        struct futex_waiter *w = &bucket->waiters[i];
        if (w->used && w->uaddr == uaddr && !w->woken && (w->bitset & bitset)) {
            w->woken = 1;
            
            acquire(&w->proc->lock);
            if (w->proc->state == SLEEPING) {
                w->proc->state = RUNNABLE;
            }
            release(&w->proc->lock);
            
            woken++;
        }
    }
    
    acquire(&futex_mgr.lock);
    futex_mgr.total_wakes += woken;
    release(&futex_mgr.lock);
    
    release(&bucket->lock);
    
    return woken;
}

// FUTEX_REQUEUE: 重新排队
int
futex_requeue(uint64 uaddr, uint64 uaddr2, int nr_wake, int nr_requeue)
{
    int hash1 = futex_hash(uaddr);
    int hash2 = futex_hash(uaddr2);
    struct futex_bucket *bucket1 = &futex_mgr.buckets[hash1];
    struct futex_bucket *bucket2 = &futex_mgr.buckets[hash2];
    
    // 避免死锁：按地址顺序获取锁
    if (hash1 < hash2) {
        acquire(&bucket1->lock);
        acquire(&bucket2->lock);
    } else if (hash1 > hash2) {
        acquire(&bucket2->lock);
        acquire(&bucket1->lock);
    } else {
        acquire(&bucket1->lock);
        bucket2 = bucket1;
    }
    
    int woken = 0;
    int requeued = 0;
    
    for (int i = 0; i < MAX_FUTEX_QUEUES / FUTEX_HASH_SIZE; i++) {
        struct futex_waiter *w = &bucket1->waiters[i];
        if (w->used && w->uaddr == uaddr && !w->woken) {
            if (woken < nr_wake) {
                // 唤醒
                w->woken = 1;
                acquire(&w->proc->lock);
                if (w->proc->state == SLEEPING) {
                    w->proc->state = RUNNABLE;
                }
                release(&w->proc->lock);
                woken++;
            } else if (requeued < nr_requeue) {
                // 重新排队到uaddr2
                w->uaddr = uaddr2;
                requeued++;
            }
        }
    }
    
    acquire(&futex_mgr.lock);
    futex_mgr.total_wakes += woken;
    futex_mgr.total_requeues += requeued;
    release(&futex_mgr.lock);
    
    if (hash1 != hash2) {
        release(&bucket2->lock);
    }
    release(&bucket1->lock);
    
    return woken + requeued;
}

// ============ 系统调用接口 ============

int
sys_futex(uint64 uaddr, int op, uint32 val, uint64 timeout, uint64 uaddr2, uint32 val3)
{
    int cmd = op & ~FUTEX_PRIVATE_FLAG;
    
    switch (cmd) {
        case FUTEX_WAIT:
            return futex_wait(uaddr, val, timeout);
        
        case FUTEX_WAKE:
            return futex_wake(uaddr, val);
        
        case FUTEX_WAKE_BITSET:
            return futex_wake_bitset(uaddr, val, val3);
        
        case FUTEX_REQUEUE:
            return futex_requeue(uaddr, uaddr2, val, (int)timeout);
        
        default:
            return -1;
    }
}

// ============ 统计输出 ============

void
futex_print_stats(void)
{
    acquire(&futex_mgr.lock);
    
    printf("\n=== Futex Statistics ===\n");
    printf("Total waits: %d\n", (int)futex_mgr.total_waits);
    printf("Total wakes: %d\n", (int)futex_mgr.total_wakes);
    printf("Total timeouts: %d\n", (int)futex_mgr.total_timeouts);
    printf("Total requeues: %d\n", (int)futex_mgr.total_requeues);
    printf("Spurious wakeups: %d\n", (int)futex_mgr.spurious_wakeups);
    printf("========================\n");
    
    release(&futex_mgr.lock);
}

int
futex_get_stats(uint64 *waits, uint64 *wakes, uint64 *timeouts)
{
    acquire(&futex_mgr.lock);
    if (waits) *waits = futex_mgr.total_waits;
    if (wakes) *wakes = futex_mgr.total_wakes;
    if (timeouts) *timeouts = futex_mgr.total_timeouts;
    release(&futex_mgr.lock);
    return 0;
}
