// cgroups - Linux 风格的资源控制组
// 限制、记录和隔离进程组的资源使用

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// ============ cgroups 常量 ============

#define MAX_CGROUPS 16
#define MAX_PROCS_PER_CGROUP 32
#define CGROUP_NAME_LEN 32

// 资源控制器类型
#define CTRL_CPU     0x01    // CPU 控制
#define CTRL_MEMORY  0x02    // 内存控制
#define CTRL_IO      0x04    // I/O 控制
#define CTRL_PIDS    0x08    // 进程数控制

// ============ cgroups 数据结构 ============

// CPU 控制器
struct cpu_controller {
    int enabled;
    int cpu_shares;         // CPU 份额 (默认1024)
    int cpu_quota;          // CPU 配额 (微秒/周期)
    int cpu_period;         // CPU 周期 (微秒)
    uint64 cpu_usage;       // 累计 CPU 使用时间
};

// 内存控制器
struct memory_controller {
    int enabled;
    uint64 memory_limit;    // 内存限制 (字节)
    uint64 memory_usage;    // 当前内存使用
    uint64 memory_max_usage;// 最大内存使用
    int oom_kill_count;     // OOM 杀死次数
};

// I/O 控制器
struct io_controller {
    int enabled;
    uint64 read_bps_limit;  // 读取速率限制
    uint64 write_bps_limit; // 写入速率限制
    uint64 read_iops_limit; // 读取 IOPS 限制
    uint64 write_iops_limit;// 写入 IOPS 限制
    uint64 bytes_read;      // 累计读取字节
    uint64 bytes_written;   // 累计写入字节
};

// 进程数控制器
struct pids_controller {
    int enabled;
    int pids_max;           // 最大进程数
    int pids_current;       // 当前进程数
};

// cgroup 结构
struct cgroup {
    int used;
    char name[CGROUP_NAME_LEN];
    int id;
    int parent_id;          // 父 cgroup ID (-1 表示根)
    
    // 控制器
    int controllers;        // 启用的控制器位图
    struct cpu_controller cpu;
    struct memory_controller memory;
    struct io_controller io;
    struct pids_controller pids;
    
    // 成员进程
    int procs[MAX_PROCS_PER_CGROUP];
    int proc_count;
    
    // 统计
    uint64 created_time;
};

// ============ cgroups 全局状态 ============

struct cgroups_state {
    struct spinlock lock;
    struct cgroup groups[MAX_CGROUPS];
    int group_count;
    int next_id;
};

static struct cgroups_state cgroups;

// ============ 初始化 ============

void
cgroups_init(void)
{
    initlock(&cgroups.lock, "cgroups");
    
    memset(cgroups.groups, 0, sizeof(cgroups.groups));
    cgroups.group_count = 0;
    cgroups.next_id = 1;
    
    // 创建根 cgroup
    struct cgroup *root = &cgroups.groups[0];
    root->used = 1;
    strncpy(root->name, "/", CGROUP_NAME_LEN);
    root->id = 0;
    root->parent_id = -1;
    root->controllers = CTRL_CPU | CTRL_MEMORY | CTRL_IO | CTRL_PIDS;
    
    // 默认 CPU 控制器设置
    root->cpu.enabled = 1;
    root->cpu.cpu_shares = 1024;
    root->cpu.cpu_quota = -1;  // 无限制
    root->cpu.cpu_period = 100000;  // 100ms
    
    // 默认内存控制器设置
    root->memory.enabled = 1;
    root->memory.memory_limit = 0;  // 无限制
    
    // 默认 I/O 控制器设置
    root->io.enabled = 1;
    root->io.read_bps_limit = 0;
    root->io.write_bps_limit = 0;
    
    // 默认进程数控制器设置
    root->pids.enabled = 1;
    root->pids.pids_max = NPROC;
    
    cgroups.group_count = 1;
    
    printf("cgroups: resource control initialized\n");
}

// ============ cgroup 管理 ============

// 创建 cgroup
int
cgroup_create(char *name, int parent_id)
{
    acquire(&cgroups.lock);
    
    // 检查父 cgroup
    struct cgroup *parent = 0;
    if (parent_id >= 0) {
        for (int i = 0; i < MAX_CGROUPS; i++) {
            if (cgroups.groups[i].used && cgroups.groups[i].id == parent_id) {
                parent = &cgroups.groups[i];
                break;
            }
        }
        if (parent == 0) {
            release(&cgroups.lock);
            return -1;
        }
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (!cgroups.groups[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&cgroups.lock);
        return -1;
    }
    
    struct cgroup *cg = &cgroups.groups[slot];
    memset(cg, 0, sizeof(struct cgroup));
    cg->used = 1;
    strncpy(cg->name, name, CGROUP_NAME_LEN);
    cg->id = cgroups.next_id++;
    cg->parent_id = parent_id;
    
    // 继承父 cgroup 的控制器设置
    if (parent) {
        cg->controllers = parent->controllers;
        cg->cpu = parent->cpu;
        cg->memory = parent->memory;
        cg->io = parent->io;
        cg->pids = parent->pids;
    } else {
        cg->controllers = CTRL_CPU | CTRL_MEMORY | CTRL_IO | CTRL_PIDS;
        cg->cpu.enabled = 1;
        cg->cpu.cpu_shares = 1024;
        cg->memory.enabled = 1;
        cg->io.enabled = 1;
        cg->pids.enabled = 1;
        cg->pids.pids_max = NPROC;
    }
    
    extern uint ticks;
    cg->created_time = ticks;
    cgroups.group_count++;
    
    release(&cgroups.lock);
    
    printf("cgroups: '%s' created (id=%d)\n", name, cg->id);
    return cg->id;
}

// 删除 cgroup
int
cgroup_delete(int cgroup_id)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            if (cgroups.groups[i].proc_count > 0) {
                release(&cgroups.lock);
                return -1;  // 还有进程，不能删除
            }
            cgroups.groups[i].used = 0;
            cgroups.group_count--;
            release(&cgroups.lock);
            return 0;
        }
    }
    
    release(&cgroups.lock);
    return -1;
}

// 将进程加入 cgroup
int
cgroup_attach(int cgroup_id, int pid)
{
    acquire(&cgroups.lock);
    
    struct cgroup *cg = 0;
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            cg = &cgroups.groups[i];
            break;
        }
    }
    
    if (cg == 0) {
        release(&cgroups.lock);
        return -1;
    }
    
    // 检查进程数限制
    if (cg->pids.enabled && cg->pids.pids_current >= cg->pids.pids_max) {
        release(&cgroups.lock);
        return -1;  // 超过进程数限制
    }
    
    // 检查是否已在此 cgroup
    for (int i = 0; i < cg->proc_count; i++) {
        if (cg->procs[i] == pid) {
            release(&cgroups.lock);
            return 0;  // 已经在此 cgroup
        }
    }
    
    if (cg->proc_count >= MAX_PROCS_PER_CGROUP) {
        release(&cgroups.lock);
        return -1;
    }
    
    cg->procs[cg->proc_count++] = pid;
    cg->pids.pids_current++;
    
    release(&cgroups.lock);
    return 0;
}

// 将进程从 cgroup 移除
int
cgroup_detach(int cgroup_id, int pid)
{
    acquire(&cgroups.lock);
    
    struct cgroup *cg = 0;
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            cg = &cgroups.groups[i];
            break;
        }
    }
    
    if (cg == 0) {
        release(&cgroups.lock);
        return -1;
    }
    
    for (int i = 0; i < cg->proc_count; i++) {
        if (cg->procs[i] == pid) {
            // 移除进程
            for (int j = i; j < cg->proc_count - 1; j++) {
                cg->procs[j] = cg->procs[j + 1];
            }
            cg->proc_count--;
            cg->pids.pids_current--;
            release(&cgroups.lock);
            return 0;
        }
    }
    
    release(&cgroups.lock);
    return -1;
}

// ============ 资源限制设置 ============

// 设置 CPU 限制
int
cgroup_set_cpu(int cgroup_id, int shares, int quota, int period)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            if (shares > 0) cgroups.groups[i].cpu.cpu_shares = shares;
            if (quota != 0) cgroups.groups[i].cpu.cpu_quota = quota;
            if (period > 0) cgroups.groups[i].cpu.cpu_period = period;
            release(&cgroups.lock);
            return 0;
        }
    }
    
    release(&cgroups.lock);
    return -1;
}

// 设置内存限制
int
cgroup_set_memory(int cgroup_id, uint64 limit)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            cgroups.groups[i].memory.memory_limit = limit;
            release(&cgroups.lock);
            return 0;
        }
    }
    
    release(&cgroups.lock);
    return -1;
}

// 设置进程数限制
int
cgroup_set_pids(int cgroup_id, int max_pids)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        if (cgroups.groups[i].used && cgroups.groups[i].id == cgroup_id) {
            cgroups.groups[i].pids.pids_max = max_pids;
            release(&cgroups.lock);
            return 0;
        }
    }
    
    release(&cgroups.lock);
    return -1;
}

// ============ 资源检查 ============

// 检查进程是否允许分配内存
int
cgroup_check_memory(int pid, uint64 size)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        struct cgroup *cg = &cgroups.groups[i];
        if (!cg->used) continue;
        
        for (int j = 0; j < cg->proc_count; j++) {
            if (cg->procs[j] == pid) {
                if (cg->memory.enabled && cg->memory.memory_limit > 0) {
                    if (cg->memory.memory_usage + size > cg->memory.memory_limit) {
                        release(&cgroups.lock);
                        return 0;  // 不允许
                    }
                }
                release(&cgroups.lock);
                return 1;  // 允许
            }
        }
    }
    
    release(&cgroups.lock);
    return 1;  // 默认允许
}

// 更新内存使用
void
cgroup_update_memory(int pid, int delta)
{
    acquire(&cgroups.lock);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        struct cgroup *cg = &cgroups.groups[i];
        if (!cg->used) continue;
        
        for (int j = 0; j < cg->proc_count; j++) {
            if (cg->procs[j] == pid) {
                if (delta > 0) {
                    cg->memory.memory_usage += delta;
                    if (cg->memory.memory_usage > cg->memory.memory_max_usage) {
                        cg->memory.memory_max_usage = cg->memory.memory_usage;
                    }
                } else {
                    if (cg->memory.memory_usage >= (uint64)(-delta)) {
                        cg->memory.memory_usage += delta;
                    } else {
                        cg->memory.memory_usage = 0;
                    }
                }
                release(&cgroups.lock);
                return;
            }
        }
    }
    
    release(&cgroups.lock);
}

// ============ 列表和统计 ============

int
cgroup_list(char *buf, int len)
{
    acquire(&cgroups.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "ID\tNAME\t\tPROCS\tMEM_LIMIT\tCPU_SHARES\n");
    
    for (int i = 0; i < MAX_CGROUPS && offset < len - 64; i++) {
        struct cgroup *cg = &cgroups.groups[i];
        if (cg->used) {
            offset += snprintf(buf + offset, len - offset,
                "%d\t%s\t\t%d\t%d KB\t\t%d\n",
                cg->id, cg->name, cg->proc_count,
                (int)(cg->memory.memory_limit / 1024),
                cg->cpu.cpu_shares);
        }
    }
    
    release(&cgroups.lock);
    return offset;
}

void
cgroups_print_stats(void)
{
    acquire(&cgroups.lock);
    
    printf("\n=== cgroups Statistics ===\n");
    printf("Total cgroups: %d\n", cgroups.group_count);
    
    for (int i = 0; i < MAX_CGROUPS; i++) {
        struct cgroup *cg = &cgroups.groups[i];
        if (cg->used) {
            printf("\n[%d] %s:\n", cg->id, cg->name);
            printf("  Processes: %d/%d\n", cg->pids.pids_current, cg->pids.pids_max);
            printf("  Memory: %d/%d KB\n", 
                   (int)(cg->memory.memory_usage / 1024),
                   (int)(cg->memory.memory_limit / 1024));
            printf("  CPU shares: %d\n", cg->cpu.cpu_shares);
        }
    }
    printf("==========================\n");
    
    release(&cgroups.lock);
}
