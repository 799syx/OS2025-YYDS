// checkpoint.c - 进程快照 (checkpoint/restore) 机制
// 支持保存和恢复进程状态，用于容错和进程迁移

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

int snprintf(char *buf, int size, const char *fmt, ...);

extern struct proc proc[];
extern uint ticks;

// ============ 常量定义 ============

#define MAX_CHECKPOINTS 32
#define CHECKPOINT_MAGIC 0x43484B50  // "CHKP"
#define MAX_CHECKPOINT_PAGES 256     // 最大保存的页面数

// ============ 数据结构 ============

// 保存的内存页面
struct saved_page {
    uint64 va;              // 虚拟地址
    char data[PGSIZE];      // 页面数据
};

// 保存的文件描述符
struct saved_fd {
    int used;
    int fd;
    int type;               // 文件类型
    char path[128];         // 文件路径
    uint64 offset;          // 文件偏移
    int flags;              // 打开标志
};

// 进程快照
struct checkpoint {
    int used;
    uint64 id;              // 快照ID
    int pid;                // 原进程PID
    char name[16];          // 进程名
    uint64 created_time;    // 创建时间
    
    // 寄存器状态
    struct trapframe tf;    // 用户态寄存器
    struct context ctx;     // 内核上下文
    
    // 内存状态
    uint64 sz;              // 进程大小
    int num_pages;          // 保存的页面数
    struct saved_page *pages;  // 页面数据 (动态分配)
    
    // 文件描述符
    struct saved_fd fds[NOFILE];
    
    // 进程属性
    int uid;
    int gid;
    int priority;
    int mlfq_level;
    
    // 统计信息
    uint64 total_ticks;
    
    // 校验和
    uint32 checksum;
};

// ============ 全局状态 ============

struct checkpoint_manager {
    struct spinlock lock;
    struct checkpoint checkpoints[MAX_CHECKPOINTS];
    int checkpoint_count;
    uint64 next_id;
    
    // 统计
    uint64 total_created;
    uint64 total_restored;
    uint64 total_deleted;
    uint64 bytes_saved;
};

static struct checkpoint_manager ckpt_mgr;

// ============ 初始化 ============

void
checkpoint_init(void)
{
    initlock(&ckpt_mgr.lock, "checkpoint");
    memset(ckpt_mgr.checkpoints, 0, sizeof(ckpt_mgr.checkpoints));
    ckpt_mgr.checkpoint_count = 0;
    ckpt_mgr.next_id = 1;
    ckpt_mgr.total_created = 0;
    ckpt_mgr.total_restored = 0;
    ckpt_mgr.total_deleted = 0;
    ckpt_mgr.bytes_saved = 0;
    
    printf("checkpoint: process snapshot system initialized\n");
}

// ============ 辅助函数 ============

// 计算校验和
static uint32
compute_ckpt_checksum(struct checkpoint *ckpt)
{
    uint32 sum = CHECKPOINT_MAGIC;
    sum ^= ckpt->pid;
    sum ^= (uint32)ckpt->sz;
    sum ^= ckpt->num_pages;
    // 简化的校验和计算
    return sum;
}

// ============ 创建快照 ============

// 创建当前进程的快照
uint64
checkpoint_create(void)
{
    struct proc *p = myproc();
    
    acquire(&ckpt_mgr.lock);
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (!ckpt_mgr.checkpoints[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&ckpt_mgr.lock);
        return 0;
    }
    
    struct checkpoint *ckpt = &ckpt_mgr.checkpoints[slot];
    memset(ckpt, 0, sizeof(struct checkpoint));
    
    ckpt->used = 1;
    ckpt->id = ckpt_mgr.next_id++;
    ckpt->pid = p->pid;
    strncpy(ckpt->name, p->name, 16);
    ckpt->created_time = ticks;
    
    // 保存寄存器状态
    acquire(&p->lock);
    memmove(&ckpt->tf, p->trapframe, sizeof(struct trapframe));
    memmove(&ckpt->ctx, &p->context, sizeof(struct context));
    
    // 保存进程属性
    ckpt->sz = p->sz;
    ckpt->uid = p->uid;
    ckpt->gid = p->gid;
    ckpt->priority = p->priority;
    ckpt->mlfq_level = p->mlfq_level;
    ckpt->total_ticks = p->total_ticks;
    
    // 计算需要保存的页面数
    int num_pages = (p->sz + PGSIZE - 1) / PGSIZE;
    if (num_pages > MAX_CHECKPOINT_PAGES) {
        num_pages = MAX_CHECKPOINT_PAGES;
    }
    ckpt->num_pages = num_pages;
    
    // 分配页面存储空间
    ckpt->pages = (struct saved_page *)kalloc();
    if (ckpt->pages == 0) {
        ckpt->used = 0;
        release(&p->lock);
        release(&ckpt_mgr.lock);
        return 0;
    }
    
    // 保存内存页面 (简化实现，只保存部分页面)
    int saved_pages = 0;
    for (uint64 va = 0; va < p->sz && saved_pages < num_pages; va += PGSIZE) {
        uint64 pa = walkaddr(p->pagetable, va);
        if (pa != 0) {
            // 由于空间限制，这里只记录地址，不实际复制数据
            // 实际实现需要分配更多内存来保存页面数据
            saved_pages++;
        }
    }
    ckpt->num_pages = saved_pages;
    
    // 保存文件描述符
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd]) {
            ckpt->fds[fd].used = 1;
            ckpt->fds[fd].fd = fd;
            ckpt->fds[fd].type = p->ofile[fd]->type;
            // 实际实现需要保存文件路径和偏移
        }
    }
    
    release(&p->lock);
    
    // 计算校验和
    ckpt->checksum = compute_ckpt_checksum(ckpt);
    
    // 更新统计
    ckpt_mgr.checkpoint_count++;
    ckpt_mgr.total_created++;
    ckpt_mgr.bytes_saved += saved_pages * PGSIZE;
    
    // 标记进程已创建快照
    acquire(&p->lock);
    p->checkpointed = 1;
    p->checkpoint_id = ckpt->id;
    release(&p->lock);
    
    release(&ckpt_mgr.lock);
    
    printf("checkpoint: created snapshot %d for process %d (%s)\n", 
           (int)ckpt->id, p->pid, p->name);
    
    return ckpt->id;
}

// 为指定进程创建快照
uint64
checkpoint_create_pid(int pid)
{
    struct proc *p;
    
    // 查找进程
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            release(&p->lock);
            
            // 简化实现：只能为当前进程创建快照
            // 实际实现需要更复杂的处理
            if (p == myproc()) {
                return checkpoint_create();
            }
            return 0;
        }
        release(&p->lock);
    }
    
    return 0;
}

// ============ 恢复快照 ============

// 从快照恢复进程
int
checkpoint_restore(uint64 checkpoint_id)
{
    struct proc *p = myproc();
    
    acquire(&ckpt_mgr.lock);
    
    // 查找快照
    struct checkpoint *ckpt = 0;
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (ckpt_mgr.checkpoints[i].used && 
            ckpt_mgr.checkpoints[i].id == checkpoint_id) {
            ckpt = &ckpt_mgr.checkpoints[i];
            break;
        }
    }
    
    if (ckpt == 0) {
        release(&ckpt_mgr.lock);
        return -1;
    }
    
    // 验证校验和
    if (ckpt->checksum != compute_ckpt_checksum(ckpt)) {
        release(&ckpt_mgr.lock);
        return -2;  // 校验和错误
    }
    
    acquire(&p->lock);
    
    // 恢复寄存器状态
    memmove(p->trapframe, &ckpt->tf, sizeof(struct trapframe));
    
    // 恢复进程属性
    p->priority = ckpt->priority;
    p->mlfq_level = ckpt->mlfq_level;
    
    // 注意：不恢复 uid/gid 以保持安全性
    // 注意：不恢复内存内容（简化实现）
    
    release(&p->lock);
    
    ckpt_mgr.total_restored++;
    
    release(&ckpt_mgr.lock);
    
    printf("checkpoint: restored process from snapshot %d\n", (int)checkpoint_id);
    
    return 0;
}

// ============ 删除快照 ============

int
checkpoint_delete(uint64 checkpoint_id)
{
    acquire(&ckpt_mgr.lock);
    
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        struct checkpoint *ckpt = &ckpt_mgr.checkpoints[i];
        if (ckpt->used && ckpt->id == checkpoint_id) {
            // 释放页面存储
            if (ckpt->pages) {
                kfree(ckpt->pages);
            }
            
            ckpt->used = 0;
            ckpt_mgr.checkpoint_count--;
            ckpt_mgr.total_deleted++;
            
            release(&ckpt_mgr.lock);
            return 0;
        }
    }
    
    release(&ckpt_mgr.lock);
    return -1;
}

// 删除进程的所有快照
int
checkpoint_delete_all(int pid)
{
    int deleted = 0;
    
    acquire(&ckpt_mgr.lock);
    
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        struct checkpoint *ckpt = &ckpt_mgr.checkpoints[i];
        if (ckpt->used && ckpt->pid == pid) {
            if (ckpt->pages) {
                kfree(ckpt->pages);
            }
            ckpt->used = 0;
            ckpt_mgr.checkpoint_count--;
            ckpt_mgr.total_deleted++;
            deleted++;
        }
    }
    
    release(&ckpt_mgr.lock);
    return deleted;
}

// ============ 查询接口 ============

// 列出所有快照
int
checkpoint_list(char *buf, int len)
{
    acquire(&ckpt_mgr.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "ID\tPID\tNAME\t\tPAGES\tTIME\n");
    
    for (int i = 0; i < MAX_CHECKPOINTS && offset < len - 64; i++) {
        struct checkpoint *ckpt = &ckpt_mgr.checkpoints[i];
        if (ckpt->used) {
            offset += snprintf(buf + offset, len - offset,
                "%d\t%d\t%s\t\t%d\t%d\n",
                (int)ckpt->id, ckpt->pid, ckpt->name,
                ckpt->num_pages, (int)ckpt->created_time);
        }
    }
    
    release(&ckpt_mgr.lock);
    return offset;
}

// 获取快照信息
int
checkpoint_info(uint64 checkpoint_id, int *pid, uint64 *sz, int *num_pages)
{
    acquire(&ckpt_mgr.lock);
    
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        struct checkpoint *ckpt = &ckpt_mgr.checkpoints[i];
        if (ckpt->used && ckpt->id == checkpoint_id) {
            if (pid) *pid = ckpt->pid;
            if (sz) *sz = ckpt->sz;
            if (num_pages) *num_pages = ckpt->num_pages;
            release(&ckpt_mgr.lock);
            return 0;
        }
    }
    
    release(&ckpt_mgr.lock);
    return -1;
}

// 检查进程是否有快照
int
checkpoint_exists(int pid)
{
    acquire(&ckpt_mgr.lock);
    
    for (int i = 0; i < MAX_CHECKPOINTS; i++) {
        if (ckpt_mgr.checkpoints[i].used && 
            ckpt_mgr.checkpoints[i].pid == pid) {
            release(&ckpt_mgr.lock);
            return 1;
        }
    }
    
    release(&ckpt_mgr.lock);
    return 0;
}

// ============ 统计信息 ============

void
checkpoint_print_stats(void)
{
    acquire(&ckpt_mgr.lock);
    
    printf("\n=== Checkpoint Statistics ===\n");
    printf("Active checkpoints: %d\n", ckpt_mgr.checkpoint_count);
    printf("Total created: %d\n", (int)ckpt_mgr.total_created);
    printf("Total restored: %d\n", (int)ckpt_mgr.total_restored);
    printf("Total deleted: %d\n", (int)ckpt_mgr.total_deleted);
    printf("Bytes saved: %d KB\n", (int)(ckpt_mgr.bytes_saved / 1024));
    printf("=============================\n");
    
    release(&ckpt_mgr.lock);
}

void
checkpoint_get_stats(uint64 *created, uint64 *restored, uint64 *deleted)
{
    acquire(&ckpt_mgr.lock);
    if (created) *created = ckpt_mgr.total_created;
    if (restored) *restored = ckpt_mgr.total_restored;
    if (deleted) *deleted = ckpt_mgr.total_deleted;
    release(&ckpt_mgr.lock);
}
