// Slab 分配器系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

void slab_print_stats(void);

// 打印 slab 统计信息
uint64
sys_slab_stats(void)
{
    slab_print_stats();
    return 0;
}
