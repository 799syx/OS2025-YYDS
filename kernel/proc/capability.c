// capability.c - 进程权能系统 (Capability-based Security)
// 实现细粒度的权限控制，类似Linux capabilities和鸿蒙OS的权能机制

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// ============ 权能定义 ============

// 基本权能位 (类似Linux CAP_*)
#define CAP_CHOWN           0   // 修改文件所有者
#define CAP_DAC_OVERRIDE    1   // 绕过文件权限检查
#define CAP_DAC_READ_SEARCH 2   // 绕过读/搜索权限
#define CAP_FOWNER          3   // 绕过文件所有者检查
#define CAP_KILL            4   // 发送信号给任意进程
#define CAP_SETGID          5   // 设置GID
#define CAP_SETUID          6   // 设置UID
#define CAP_NET_BIND        7   // 绑定特权端口
#define CAP_NET_RAW         8   // 使用原始套接字
#define CAP_SYS_BOOT        9   // 重启系统
#define CAP_SYS_ADMIN       10  // 系统管理操作
#define CAP_SYS_NICE        11  // 修改进程优先级
#define CAP_SYS_RESOURCE    12  // 修改资源限制
#define CAP_SYS_TIME        13  // 修改系统时间
#define CAP_SYS_PTRACE      14  // 跟踪任意进程
#define CAP_MKNOD           15  // 创建设备文件
#define CAP_AUDIT_WRITE     16  // 写入审计日志
#define CAP_SETFCAP         17  // 设置文件权能

// 鸿蒙风格的扩展权能
#define CAP_ABILITY_START   18  // 启动Ability
#define CAP_ABILITY_BG      19  // 后台运行Ability
#define CAP_DISTRIBUTED     20  // 分布式操作
#define CAP_DEVICE_ACCESS   21  // 设备访问
#define CAP_SENSOR_ACCESS   22  // 传感器访问
#define CAP_LOCATION        23  // 位置信息
#define CAP_CAMERA          24  // 摄像头访问
#define CAP_MICROPHONE      25  // 麦克风访问
#define CAP_STORAGE_READ    26  // 存储读取
#define CAP_STORAGE_WRITE   27  // 存储写入
#define CAP_NETWORK         28  // 网络访问
#define CAP_BLUETOOTH       29  // 蓝牙访问

#define CAP_MAX             32  // 最大权能数

// 权能集合类型
#define CAP_EFFECTIVE   0   // 有效权能集
#define CAP_PERMITTED   1   // 许可权能集
#define CAP_INHERITABLE 2   // 可继承权能集
#define CAP_BOUNDING    3   // 边界权能集

// ============ 数据结构 ============

// 进程权能结构
struct proc_caps {
    uint32 effective;    // 当前有效的权能
    uint32 permitted;    // 允许拥有的权能
    uint32 inheritable;  // 可继承给子进程的权能
    uint32 bounding;     // 权能边界集
};

// 权能审计记录
struct cap_audit {
    int pid;
    int cap;
    int granted;
    uint64 timestamp;
};

#define MAX_CAP_AUDIT 64
static struct cap_audit cap_audit_log[MAX_CAP_AUDIT];
static int cap_audit_idx = 0;
static struct spinlock cap_audit_lock;

// ============ 全局状态 ============

static struct {
    struct spinlock lock;
    int initialized;
    uint32 default_caps;        // 默认权能
    uint32 root_caps;           // root用户权能
    uint64 total_checks;
    uint64 total_granted;
    uint64 total_denied;
} cap_mgr;

// ============ 初始化 ============

void
capability_init(void)
{
    initlock(&cap_mgr.lock, "capability");
    initlock(&cap_audit_lock, "cap_audit");
    
    cap_mgr.initialized = 1;
    // 默认权能：基本操作
    cap_mgr.default_caps = (1 << CAP_STORAGE_READ) | (1 << CAP_NETWORK);
    // root权能：全部
    cap_mgr.root_caps = 0xFFFFFFFF;
    cap_mgr.total_checks = 0;
    cap_mgr.total_granted = 0;
    cap_mgr.total_denied = 0;
    
    memset(cap_audit_log, 0, sizeof(cap_audit_log));
    
    printf("capability: security system initialized\n");
}

// ============ 权能检查 ============

// 检查进程是否拥有指定权能
int
cap_check(int pid, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return 0;
    
    struct proc *p;
    int has_cap = 0;
    
    // 查找进程
    extern struct proc proc[];
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            // root用户拥有所有权能
            if (p->uid == 0) {
                has_cap = 1;
            } else {
                // 检查有效权能集
                has_cap = (p->caps_effective & (1 << cap)) != 0;
            }
            release(&p->lock);
            break;
        }
        release(&p->lock);
    }
    
    // 更新统计
    acquire(&cap_mgr.lock);
    cap_mgr.total_checks++;
    if (has_cap) {
        cap_mgr.total_granted++;
    } else {
        cap_mgr.total_denied++;
    }
    release(&cap_mgr.lock);
    
    // 审计记录
    acquire(&cap_audit_lock);
    struct cap_audit *audit = &cap_audit_log[cap_audit_idx % MAX_CAP_AUDIT];
    audit->pid = pid;
    audit->cap = cap;
    audit->granted = has_cap;
    extern uint ticks;
    audit->timestamp = ticks;
    cap_audit_idx++;
    release(&cap_audit_lock);
    
    return has_cap;
}

// 检查当前进程是否拥有指定权能
int
cap_capable(int cap)
{
    struct proc *p = myproc();
    return cap_check(p->pid, cap);
}

// ============ 权能设置 ============

// 设置进程权能
int
cap_set(int pid, int cap_type, uint32 caps)
{
    if (cap_type < 0 || cap_type > CAP_BOUNDING) return -1;
    
    // 只有root或拥有CAP_SETFCAP的进程可以设置权能
    struct proc *cur = myproc();
    if (cur->uid != 0 && !cap_capable(CAP_SETFCAP)) {
        return -1;
    }
    
    extern struct proc proc[];
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            switch (cap_type) {
                case CAP_EFFECTIVE:
                    // 有效权能不能超过许可权能
                    p->caps_effective = caps & p->caps_permitted;
                    break;
                case CAP_PERMITTED:
                    // 许可权能不能超过边界权能
                    p->caps_permitted = caps & p->caps_bounding;
                    break;
                case CAP_INHERITABLE:
                    p->caps_inheritable = caps;
                    break;
                case CAP_BOUNDING:
                    // 边界权能只能减少不能增加
                    p->caps_bounding &= caps;
                    break;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 获取进程权能
int
cap_get(int pid, int cap_type, uint32 *caps)
{
    if (cap_type < 0 || cap_type > CAP_BOUNDING || !caps) return -1;
    
    extern struct proc proc[];
    struct proc *p;
    
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid && p->state != UNUSED) {
            switch (cap_type) {
                case CAP_EFFECTIVE:
                    *caps = p->caps_effective;
                    break;
                case CAP_PERMITTED:
                    *caps = p->caps_permitted;
                    break;
                case CAP_INHERITABLE:
                    *caps = p->caps_inheritable;
                    break;
                case CAP_BOUNDING:
                    *caps = p->caps_bounding;
                    break;
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    
    return -1;
}

// 添加单个权能
int
cap_raise(int pid, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return -1;
    
    uint32 current;
    if (cap_get(pid, CAP_EFFECTIVE, &current) < 0) return -1;
    
    return cap_set(pid, CAP_EFFECTIVE, current | (1 << cap));
}

// 移除单个权能
int
cap_drop(int pid, int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return -1;
    
    uint32 current;
    if (cap_get(pid, CAP_EFFECTIVE, &current) < 0) return -1;
    
    return cap_set(pid, CAP_EFFECTIVE, current & ~(1 << cap));
}

// ============ 权能继承 ============

// 初始化新进程的权能 (fork时调用)
void
cap_fork_init(struct proc *child, struct proc *parent)
{
    // 子进程继承父进程的可继承权能
    child->caps_effective = parent->caps_inheritable & parent->caps_permitted;
    child->caps_permitted = parent->caps_inheritable & parent->caps_bounding;
    child->caps_inheritable = parent->caps_inheritable;
    child->caps_bounding = parent->caps_bounding;
}

// exec时重新计算权能
void
cap_exec_init(struct proc *p)
{
    // exec后，有效权能 = 许可权能 & 可继承权能
    p->caps_effective = p->caps_permitted & p->caps_inheritable;
}

// ============ 权能名称 ============

static const char *cap_names[] = {
    "CAP_CHOWN", "CAP_DAC_OVERRIDE", "CAP_DAC_READ_SEARCH", "CAP_FOWNER",
    "CAP_KILL", "CAP_SETGID", "CAP_SETUID", "CAP_NET_BIND",
    "CAP_NET_RAW", "CAP_SYS_BOOT", "CAP_SYS_ADMIN", "CAP_SYS_NICE",
    "CAP_SYS_RESOURCE", "CAP_SYS_TIME", "CAP_SYS_PTRACE", "CAP_MKNOD",
    "CAP_AUDIT_WRITE", "CAP_SETFCAP", "CAP_ABILITY_START", "CAP_ABILITY_BG",
    "CAP_DISTRIBUTED", "CAP_DEVICE_ACCESS", "CAP_SENSOR_ACCESS", "CAP_LOCATION",
    "CAP_CAMERA", "CAP_MICROPHONE", "CAP_STORAGE_READ", "CAP_STORAGE_WRITE",
    "CAP_NETWORK", "CAP_BLUETOOTH", "RESERVED", "RESERVED"
};

const char*
cap_name(int cap)
{
    if (cap < 0 || cap >= CAP_MAX) return "UNKNOWN";
    return cap_names[cap];
}

// ============ 统计和调试 ============

void
cap_print_stats(void)
{
    acquire(&cap_mgr.lock);
    
    printf("\n=== Capability Statistics ===\n");
    printf("Total checks: %d\n", (int)cap_mgr.total_checks);
    printf("Granted: %d\n", (int)cap_mgr.total_granted);
    printf("Denied: %d\n", (int)cap_mgr.total_denied);
    printf("=============================\n");
    
    release(&cap_mgr.lock);
}

int
cap_list_process(int pid, char *buf, int len)
{
    uint32 eff, perm, inh, bound;
    
    if (cap_get(pid, CAP_EFFECTIVE, &eff) < 0) {
        return snprintf(buf, len, "Process %d not found\n", pid);
    }
    cap_get(pid, CAP_PERMITTED, &perm);
    cap_get(pid, CAP_INHERITABLE, &inh);
    cap_get(pid, CAP_BOUNDING, &bound);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "Process %d capabilities:\n", pid);
    offset += snprintf(buf + offset, len - offset,
        "  Effective:   0x%x\n", eff);
    offset += snprintf(buf + offset, len - offset,
        "  Permitted:   0x%x\n", perm);
    offset += snprintf(buf + offset, len - offset,
        "  Inheritable: 0x%x\n", inh);
    offset += snprintf(buf + offset, len - offset,
        "  Bounding:    0x%x\n", bound);
    
    // 列出有效权能
    offset += snprintf(buf + offset, len - offset, "  Active caps: ");
    for (int i = 0; i < CAP_MAX && offset < len - 32; i++) {
        if (eff & (1 << i)) {
            offset += snprintf(buf + offset, len - offset, "%s ", cap_names[i]);
        }
    }
    offset += snprintf(buf + offset, len - offset, "\n");
    
    return offset;
}
