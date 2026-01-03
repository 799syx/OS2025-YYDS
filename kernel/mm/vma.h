// VMA (Virtual Memory Area) 管理头文件

#ifndef _VMA_H_
#define _VMA_H_

#include "types.h"

// VMA 标志
#define VMA_READ    0x1
#define VMA_WRITE   0x2
#define VMA_EXEC    0x4
#define VMA_SHARED  0x8
#define VMA_PRIVATE 0x10
#define VMA_ANON    0x20
#define VMA_FILE    0x40
#define VMA_STACK   0x80
#define VMA_HEAP    0x100
#define VMA_GROWSDOWN 0x200

// 初始化 VMA 子系统
void vma_init(void);

// 创建新的 VMA 映射
int vma_map(struct proc *p, uint64 addr, uint64 len, int prot, int flags, 
            struct file *f, uint64 offset);

// 取消 VMA 映射
int vma_unmap(struct proc *p, uint64 addr, uint64 len);

// 处理 VMA 缺页异常
int vma_fault(struct proc *p, uint64 va, int write);

// 复制进程的 VMA（用于 fork）
int vma_copy(struct proc *dst, struct proc *src);

// 释放进程的所有 VMA
void vma_free_all(struct proc *p);

// 打印进程的 VMA 信息
void vma_print(struct proc *p);

// 获取 VMA 统计信息
void vma_get_stats(uint64 *total_vmas, uint64 *total_mapped, 
                   uint64 *page_faults, uint64 *cow_faults);

// 记录 COW 缺页
void vma_record_cow_fault(void);

#endif // _VMA_H_
