// HMDFS 系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs/hmdfs.h"

// 注册设备
uint64
sys_hmdfs_register_device(void)
{
    uint64 name_addr, uuid_addr;
    int type;
    
    if (argaddr(0, &name_addr) < 0)
        return -1;
    if (argaddr(1, &uuid_addr) < 0)
        return -1;
    if (argint(2, &type) < 0)
        return -1;
    
    char name[32], uuid[40];
    if (fetchstr(name_addr, name, sizeof(name)) < 0)
        return -1;
    if (fetchstr(uuid_addr, uuid, sizeof(uuid)) < 0)
        return -1;
    
    return hmdfs_register_device(name, uuid, type);
}

// 设备下线
uint64
sys_hmdfs_device_offline(void)
{
    int device_id;
    if (argint(0, &device_id) < 0)
        return -1;
    return hmdfs_device_offline(device_id);
}

// 列出设备
uint64
sys_hmdfs_list_devices(void)
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
    int n = hmdfs_list_devices(buf, len);
    
    struct proc *p = myproc();
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 共享文件
uint64
sys_hmdfs_share(void)
{
    uint64 path_addr;
    if (argaddr(0, &path_addr) < 0)
        return -1;
    
    char path[128];
    if (fetchstr(path_addr, path, sizeof(path)) < 0)
        return -1;
    
    return hmdfs_share_file(path);
}

// 取消共享
uint64
sys_hmdfs_unshare(void)
{
    uint64 path_addr;
    if (argaddr(0, &path_addr) < 0)
        return -1;
    
    char path[128];
    if (fetchstr(path_addr, path, sizeof(path)) < 0)
        return -1;
    
    return hmdfs_unshare_file(path);
}

// 列出共享文件
uint64
sys_hmdfs_list_shared(void)
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
    int n = hmdfs_list_shared(buf, len);
    
    struct proc *p = myproc();
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 同步
uint64
sys_hmdfs_sync(void)
{
    return hmdfs_sync();
}

// 打印统计
uint64
sys_hmdfs_stats(void)
{
    hmdfs_print_stats();
    return 0;
}
