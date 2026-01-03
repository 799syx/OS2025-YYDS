// Binder IPC 系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc/binder.h"

// 注册服务
uint64
sys_binder_register(void)
{
    uint64 name_addr;
    uint64 ptr;
    
    if (argaddr(0, &name_addr) < 0)
        return -1;
    if (argaddr(1, &ptr) < 0)
        return -1;
    
    char name[32];
    if (fetchstr(name_addr, name, sizeof(name)) < 0)
        return -1;
    
    return binder_register_service(name, ptr);
}

// 查找服务
uint64
sys_binder_lookup(void)
{
    uint64 name_addr;
    
    if (argaddr(0, &name_addr) < 0)
        return -1;
    
    char name[32];
    if (fetchstr(name_addr, name, sizeof(name)) < 0)
        return -1;
    
    return binder_lookup_service(name);
}

// 释放服务
uint64
sys_binder_release(void)
{
    int handle;
    if (argint(0, &handle) < 0)
        return -1;
    return binder_release_service(handle);
}

// 列出服务
uint64
sys_binder_list(void)
{
    uint64 buf_addr;
    int len;
    
    if (argaddr(0, &buf_addr) < 0)
        return -1;
    if (argint(1, &len) < 0)
        return -1;
    
    if (len <= 0 || len > 512)
        return -1;
    
    char buf[512];
    int n = binder_list_services(buf, len);
    
    struct proc *p = myproc();
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 打印统计
uint64
sys_binder_stats(void)
{
    binder_print_stats();
    return 0;
}
