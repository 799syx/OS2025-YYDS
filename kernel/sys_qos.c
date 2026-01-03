// QoS 系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc/qos.h"

// 设置当前进程的 QoS 级别
uint64
sys_qos_set(void)
{
    int level;
    if (argint(0, &level) < 0)
        return -1;
    
    struct proc *p = myproc();
    return qos_set_level(p, level);
}

// 获取当前进程的 QoS 级别
uint64
sys_qos_get(void)
{
    struct proc *p = myproc();
    return qos_get_level(p);
}

// 设置进程的截止时间
uint64
sys_qos_set_deadline(void)
{
    uint64 deadline;
    if (argaddr(0, &deadline) < 0)
        return -1;
    
    struct proc *p = myproc();
    return qos_set_deadline(p, deadline);
}

// 获取 QoS 统计信息
uint64
sys_qos_stats(void)
{
    uint64 user_total, user_boosts, user_misses, user_switches;
    
    if (argaddr(0, &user_total) < 0)
        return -1;
    if (argaddr(1, &user_boosts) < 0)
        return -1;
    if (argaddr(2, &user_misses) < 0)
        return -1;
    if (argaddr(3, &user_switches) < 0)
        return -1;
    
    uint64 total, boosts, misses, switches;
    qos_get_stats(&total, &boosts, &misses, &switches);
    
    struct proc *p = myproc();
    if (user_total && copyout(p->pagetable, user_total, (char*)&total, sizeof(total)) < 0)
        return -1;
    if (user_boosts && copyout(p->pagetable, user_boosts, (char*)&boosts, sizeof(boosts)) < 0)
        return -1;
    if (user_misses && copyout(p->pagetable, user_misses, (char*)&misses, sizeof(misses)) < 0)
        return -1;
    if (user_switches && copyout(p->pagetable, user_switches, (char*)&switches, sizeof(switches)) < 0)
        return -1;
    
    return 0;
}

// 打印 QoS 统计（调试用）
uint64
sys_qos_print(void)
{
    qos_print_stats();
    return 0;
}
