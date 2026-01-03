// sysfs 系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs/sysfs.h"

// 读取 sysfs 文件
uint64
sys_sysfs_read(void)
{
    uint64 path_addr, buf_addr;
    int len;
    
    if (argaddr(0, &path_addr) < 0)
        return -1;
    if (argaddr(1, &buf_addr) < 0)
        return -1;
    if (argint(2, &len) < 0)
        return -1;
    
    if (len <= 0 || len > 4096)
        return -1;
    
    struct proc *p = myproc();
    
    // 读取路径
    char path[128];
    if (fetchstr(path_addr, path, sizeof(path)) < 0)
        return -1;
    
    // 读取 sysfs 内容
    char buf[4096];
    int n = sysfs_read(path, buf, len);
    if (n < 0)
        return -1;
    
    // 复制到用户空间
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 列出 sysfs 目录
uint64
sys_sysfs_list(void)
{
    uint64 path_addr, buf_addr;
    int len;
    
    if (argaddr(0, &path_addr) < 0)
        return -1;
    if (argaddr(1, &buf_addr) < 0)
        return -1;
    if (argint(2, &len) < 0)
        return -1;
    
    if (len <= 0 || len > 4096)
        return -1;
    
    struct proc *p = myproc();
    
    // 读取路径
    char path[128];
    if (fetchstr(path_addr, path, sizeof(path)) < 0)
        return -1;
    
    // 列出目录
    char buf[4096];
    int n = sysfs_list(path, buf, len);
    if (n < 0)
        return -1;
    
    // 复制到用户空间
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}
