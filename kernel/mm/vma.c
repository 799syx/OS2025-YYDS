// VMA (Virtual Memory Area) 管理
// 增强的虚拟内存区域管理，支持更灵活的内存映射

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

// 保护标志（与 mmap 兼容）
#ifndef PROT_READ
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4
#endif

#ifndef MAP_SHARED
#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#endif

// VMA 标志
#define VMA_READ    0x1
#define VMA_WRITE   0x2
#define VMA_EXEC    0x4
#define VMA_SHARED  0x8
#define VMA_PRIVATE 0x10
#define VMA_ANON    0x20    // 匿名映射
#define VMA_FILE    0x40    // 文件映射
#define VMA_STACK   0x80    // 栈区域
#define VMA_HEAP    0x100   // 堆区域
#define VMA_GROWSDOWN 0x200 // 向下增长（栈）

// VMA 类型
enum vma_type {
    VMA_TYPE_NONE = 0,
    VMA_TYPE_CODE,      // 代码段
    VMA_TYPE_DATA,      // 数据段
    VMA_TYPE_BSS,       // BSS段
    VMA_TYPE_HEAP,      // 堆
    VMA_TYPE_STACK,     // 栈
    VMA_TYPE_MMAP,      // mmap映射
    VMA_TYPE_SHARED,    // 共享内存
};

// 扩展的 VMA 结构
struct vma_ext {
    uint64 start;           // 起始虚拟地址
    uint64 end;             // 结束虚拟地址
    uint64 flags;           // 标志
    enum vma_type type;     // 类型
    struct file *file;      // 关联文件（如果有）
    uint64 file_offset;     // 文件偏移
    uint64 file_size;       // 文件映射大小
    int refcount;           // 引用计数（用于共享）
    struct vma_ext *next;   // 链表下一个
};

// VMA 管理器
struct vma_manager {
    struct spinlock lock;
    struct vma_ext *vmas;   // VMA 链表
    int count;              // VMA 数量
    uint64 brk;             // 当前堆顶
    uint64 stack_top;       // 栈顶
};

// 全局 VMA 缓存（用于快速分配）
#define VMA_CACHE_SIZE 64
static struct vma_ext vma_cache[VMA_CACHE_SIZE];
static struct spinlock vma_cache_lock;
static int vma_cache_used[VMA_CACHE_SIZE];

// VMA 统计
struct vma_stats {
    uint64 total_vmas;
    uint64 total_mapped;
    uint64 page_faults;
    uint64 cow_faults;
};
static struct vma_stats vma_stats;
static struct spinlock vma_stats_lock;

// 初始化 VMA 子系统
void
vma_init(void)
{
    initlock(&vma_cache_lock, "vma_cache");
    initlock(&vma_stats_lock, "vma_stats");
    
    memset(vma_cache, 0, sizeof(vma_cache));
    memset(vma_cache_used, 0, sizeof(vma_cache_used));
    memset(&vma_stats, 0, sizeof(vma_stats));
    
    printf("vma: initialized with cache size %d\n", VMA_CACHE_SIZE);
}

// 注：vma_alloc 和 vma_free 函数已移除，使用进程内置的 vma 数组

// 查找包含指定地址的 VMA
struct vma_ext*
vma_find(struct proc *p, uint64 addr)
{
    // 使用进程的 vm_area 数组
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used && 
            addr >= p->vma[i].addr && 
            addr < p->vma[i].addr + p->vma[i].len) {
            // 转换为 vma_ext 格式返回（简化处理）
            static struct vma_ext temp;
            temp.start = p->vma[i].addr;
            temp.end = p->vma[i].addr + p->vma[i].len;
            temp.flags = p->vma[i].prot;
            temp.file = p->vma[i].vfile;
            temp.file_offset = p->vma[i].offset;
            return &temp;
        }
    }
    return 0;
}

// 创建新的 VMA 映射
int
vma_map(struct proc *p, uint64 addr, uint64 len, int prot, int flags, 
        struct file *f, uint64 offset)
{
    // 查找空闲的 VMA 槽位
    int slot = -1;
    for (int i = 0; i < NVMA; i++) {
        if (!p->vma[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0)
        return -1;  // 没有空闲槽位
    
    // 检查地址范围是否与现有映射冲突
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used) {
            uint64 vma_start = p->vma[i].addr;
            uint64 vma_end = vma_start + p->vma[i].len;
            if (addr < vma_end && addr + len > vma_start) {
                return -1;  // 地址冲突
            }
        }
    }
    
    // 设置 VMA
    p->vma[slot].used = 1;
    p->vma[slot].addr = addr;
    p->vma[slot].len = len;
    p->vma[slot].prot = prot;
    p->vma[slot].flags = flags;
    p->vma[slot].vfile = f;
    p->vma[slot].offset = offset;
    
    if (f) {
        filedup(f);
    }
    
    acquire(&vma_stats_lock);
    vma_stats.total_mapped += len;
    release(&vma_stats_lock);
    
    return 0;
}

// 取消 VMA 映射
int
vma_unmap(struct proc *p, uint64 addr, uint64 len)
{
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used && p->vma[i].addr == addr) {
            // 如果有关联文件且是共享映射，写回脏页
            if (p->vma[i].vfile && (p->vma[i].flags & MAP_SHARED)) {
                // 写回文件（简化处理）
                // 实际实现需要遍历页表找到脏页
            }
            
            // 释放物理页面
            uint64 start = PGROUNDDOWN(addr);
            uint64 end = PGROUNDUP(addr + len);
            for (uint64 va = start; va < end; va += PGSIZE) {
                pte_t *pte = walk(p->pagetable, va, 0);
                if (pte && (*pte & PTE_V)) {
                    uint64 pa = PTE2PA(*pte);
                    kfree((void *)pa);
                    *pte = 0;
                }
            }
            
            if (p->vma[i].vfile) {
                fileclose(p->vma[i].vfile);
            }
            
            acquire(&vma_stats_lock);
            if (vma_stats.total_mapped >= p->vma[i].len)
                vma_stats.total_mapped -= p->vma[i].len;
            release(&vma_stats_lock);
            
            p->vma[i].used = 0;
            return 0;
        }
    }
    
    return -1;  // 未找到
}

// 处理 VMA 缺页异常
int
vma_fault(struct proc *p, uint64 va, int write)
{
    acquire(&vma_stats_lock);
    vma_stats.page_faults++;
    release(&vma_stats_lock);
    
    // 查找对应的 VMA
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used &&
            va >= p->vma[i].addr &&
            va < p->vma[i].addr + p->vma[i].len) {
            
            // 检查权限
            if (write && !(p->vma[i].prot & PROT_WRITE))
                return -1;
            
            // 分配物理页面
            char *mem = kalloc();
            if (mem == 0)
                return -1;
            
            memset(mem, 0, PGSIZE);
            
            // 如果是文件映射，读取文件内容
            if (p->vma[i].vfile) {
                uint64 offset = p->vma[i].offset + (PGROUNDDOWN(va) - p->vma[i].addr);
                ilock(p->vma[i].vfile->ip);
                readi(p->vma[i].vfile->ip, 0, (uint64)mem, offset, PGSIZE);
                iunlock(p->vma[i].vfile->ip);
            }
            
            // 设置页表项权限
            int perm = PTE_U;
            if (p->vma[i].prot & PROT_READ)
                perm |= PTE_R;
            if (p->vma[i].prot & PROT_WRITE)
                perm |= PTE_W;
            if (p->vma[i].prot & PROT_EXEC)
                perm |= PTE_X;
            
            // 映射页面
            if (mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, 
                        (uint64)mem, perm) != 0) {
                kfree(mem);
                return -1;
            }
            
            return 0;
        }
    }
    
    return -1;  // 未找到对应 VMA
}

// 复制进程的 VMA（用于 fork）
int
vma_copy(struct proc *dst, struct proc *src)
{
    for (int i = 0; i < NVMA; i++) {
        if (src->vma[i].used) {
            dst->vma[i] = src->vma[i];
            if (dst->vma[i].vfile) {
                filedup(dst->vma[i].vfile);
            }
        }
    }
    return 0;
}

// 释放进程的所有 VMA
void
vma_free_all(struct proc *p)
{
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used) {
            if (p->vma[i].vfile) {
                fileclose(p->vma[i].vfile);
            }
            p->vma[i].used = 0;
        }
    }
}

// 打印进程的 VMA 信息
void
vma_print(struct proc *p)
{
    printf("\n=== VMA for process %d (%s) ===\n", p->pid, p->name);
    printf("%-12s %-12s %-6s %-8s\n", "START", "END", "PROT", "FLAGS");
    
    for (int i = 0; i < NVMA; i++) {
        if (p->vma[i].used) {
            char prot[4] = "---";
            if (p->vma[i].prot & PROT_READ) prot[0] = 'r';
            if (p->vma[i].prot & PROT_WRITE) prot[1] = 'w';
            if (p->vma[i].prot & PROT_EXEC) prot[2] = 'x';
            
            printf("0x%x 0x%x %s    0x%x\n",
                   (uint)p->vma[i].addr,
                   (uint)(p->vma[i].addr + p->vma[i].len),
                   prot,
                   p->vma[i].flags);
        }
    }
    printf("==============================\n");
}

// 获取 VMA 统计信息
void
vma_get_stats(uint64 *total_vmas, uint64 *total_mapped, 
              uint64 *page_faults, uint64 *cow_faults)
{
    acquire(&vma_stats_lock);
    if (total_vmas) *total_vmas = vma_stats.total_vmas;
    if (total_mapped) *total_mapped = vma_stats.total_mapped;
    if (page_faults) *page_faults = vma_stats.page_faults;
    if (cow_faults) *cow_faults = vma_stats.cow_faults;
    release(&vma_stats_lock);
}

// 记录 COW 缺页
void
vma_record_cow_fault(void)
{
    acquire(&vma_stats_lock);
    vma_stats.cow_faults++;
    release(&vma_stats_lock);
}
