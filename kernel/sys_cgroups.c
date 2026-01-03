// cgroups 系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc/cgroups.h"

// 创建 cgroup
uint64
sys_cgroup_create(void)
{
    uint64 name_addr;
    int parent_id;
    
    if (argaddr(0, &name_addr) < 0)
        return -1;
    if (argint(1, &parent_id) < 0)
        return -1;
    
    char name[32];
    if (fetchstr(name_addr, name, sizeof(name)) < 0)
        return -1;
    
    return cgroup_create(name, parent_id);
}

// 删除 cgroup
uint64
sys_cgroup_delete(void)
{
    int cgroup_id;
    if (argint(0, &cgroup_id) < 0)
        return -1;
    return cgroup_delete(cgroup_id);
}

// 加入 cgroup
uint64
sys_cgroup_attach(void)
{
    int cgroup_id, pid;
    if (argint(0, &cgroup_id) < 0)
        return -1;
    if (argint(1, &pid) < 0)
        return -1;
    return cgroup_attach(cgroup_id, pid);
}

// 设置内存限制
uint64
sys_cgroup_set_memory(void)
{
    int cgroup_id;
    uint64 limit;
    
    if (argint(0, &cgroup_id) < 0)
        return -1;
    if (argaddr(1, &limit) < 0)
        return -1;
    
    return cgroup_set_memory(cgroup_id, limit);
}

// 设置 CPU 限制
uint64
sys_cgroup_set_cpu(void)
{
    int cgroup_id, shares;
    
    if (argint(0, &cgroup_id) < 0)
        return -1;
    if (argint(1, &shares) < 0)
        return -1;
    
    return cgroup_set_cpu(cgroup_id, shares, -1, 100000);
}

// 列出 cgroups
uint64
sys_cgroup_list(void)
{
    uint64 buf_addr;
    int len;
    
    if (argaddr(0, &buf_addr) < 0)
        return -1;
    if (argint(1, &len) < 0)
        return -1;
    
    if (len <= 0 || len > 4096)
        return -1;
    
    char buf[4096];
    int n = cgroup_list(buf, len);
    
    struct proc *p = myproc();
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 打印统计
uint64
sys_cgroups_stats(void)
{
    cgroups_print_stats();
    return 0;
}
