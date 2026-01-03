// Ability 框架系统调用实现

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "proc/ability.h"

// 注册 Ability
uint64
sys_ability_register(void)
{
    uint64 bundle_addr, name_addr;
    int type;
    
    if (argaddr(0, &bundle_addr) < 0)
        return -1;
    if (argaddr(1, &name_addr) < 0)
        return -1;
    if (argint(2, &type) < 0)
        return -1;
    
    char bundle[64], name[32];
    if (fetchstr(bundle_addr, bundle, sizeof(bundle)) < 0)
        return -1;
    if (fetchstr(name_addr, name, sizeof(name)) < 0)
        return -1;
    
    return ability_register(bundle, name, type);
}

// 启动 Ability
uint64
sys_ability_start(void)
{
    int ability_id;
    if (argint(0, &ability_id) < 0)
        return -1;
    return ability_start(ability_id);
}

// 停止 Ability
uint64
sys_ability_stop(void)
{
    int ability_id;
    if (argint(0, &ability_id) < 0)
        return -1;
    return ability_stop(ability_id);
}

// 销毁 Ability
uint64
sys_ability_destroy(void)
{
    int ability_id;
    if (argint(0, &ability_id) < 0)
        return -1;
    return ability_destroy(ability_id);
}

// 返回上一页
uint64
sys_ability_back(void)
{
    return ability_back();
}

// 列出 Abilities
uint64
sys_ability_list(void)
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
    int n = ability_list(buf, len);
    
    struct proc *p = myproc();
    if (copyout(p->pagetable, buf_addr, buf, n) < 0)
        return -1;
    
    return n;
}

// 打印统计
uint64
sys_ability_stats(void)
{
    ability_print_stats();
    return 0;
}
