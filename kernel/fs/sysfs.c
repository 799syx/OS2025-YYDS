// sysfs - 以文件形式暴露内核信息
// 提供 /sys 虚拟文件系统，让用户程序可以读取内核状态

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "fs.h"
#include "file.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// 使用 strncmp 替代 strcmp
static int
str_equal(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        if (*s1 != *s2) return 0;
        s1++; s2++;
    }
    return *s1 == *s2;
}

// sysfs 节点类型
enum sysfs_type {
    SYSFS_DIR,      // 目录
    SYSFS_FILE,     // 普通文件
    SYSFS_LINK,     // 符号链接
};

// sysfs 节点
struct sysfs_node {
    char name[32];
    enum sysfs_type type;
    int (*read)(char *buf, int len);   // 读取回调
    int (*write)(char *buf, int len);  // 写入回调
    struct sysfs_node *parent;
    struct sysfs_node *children;
    struct sysfs_node *next;
    int mode;       // 权限
};

// sysfs 根目录
static struct sysfs_node sysfs_root;
static struct spinlock sysfs_lock;

// 预定义的 sysfs 节点
static struct sysfs_node sysfs_kernel;
static struct sysfs_node sysfs_kernel_version;
static struct sysfs_node sysfs_kernel_hostname;
static struct sysfs_node sysfs_kernel_uptime;

static struct sysfs_node sysfs_mem;
static struct sysfs_node sysfs_mem_total;
static struct sysfs_node sysfs_mem_free;
static struct sysfs_node sysfs_mem_used;

static struct sysfs_node sysfs_cpu;
static struct sysfs_node sysfs_cpu_count;
static struct sysfs_node sysfs_cpu_info;

static struct sysfs_node sysfs_proc;

// 外部声明
extern uint ticks;
extern char hostname[];

// ============ 读取回调函数 ============

// 读取内核版本
static int
read_kernel_version(char *buf, int len)
{
    return snprintf(buf, len, "YYDS-OS v1.0 (RISC-V 64-bit)\n");
}

// 读取主机名
static int
read_kernel_hostname(char *buf, int len)
{
    extern char hostname[];
    return snprintf(buf, len, "%s\n", hostname);
}

// 读取运行时间
static int
read_kernel_uptime(char *buf, int len)
{
    uint64 secs = ticks / 100;  // 假设 100 ticks/秒
    uint64 mins = secs / 60;
    uint64 hours = mins / 60;
    uint64 days = hours / 24;
    
    return snprintf(buf, len, 
        "uptime: %d days, %d:%02d:%02d\n"
        "ticks: %d\n",
        (int)days, (int)(hours % 24), (int)(mins % 60), (int)(secs % 60),
        (int)ticks);
}

// 读取总内存
static int
read_mem_total(char *buf, int len)
{
    uint64 total = PHYSTOP - KERNBASE;
    return snprintf(buf, len, "%d KB\n", (int)(total / 1024));
}

// 读取空闲内存
static int
read_mem_free(char *buf, int len)
{
    uint64 free_bytes;
    freebytes(&free_bytes);
    return snprintf(buf, len, "%d KB\n", (int)(free_bytes / 1024));
}

// 读取已用内存
static int
read_mem_used(char *buf, int len)
{
    uint64 total = PHYSTOP - KERNBASE;
    uint64 free_bytes;
    freebytes(&free_bytes);
    uint64 used = total - free_bytes;
    return snprintf(buf, len, "%d KB\n", (int)(used / 1024));
}

// 读取 CPU 数量
static int
read_cpu_count(char *buf, int len)
{
    return snprintf(buf, len, "%d\n", NCPU);
}

// 读取 CPU 信息
static int
read_cpu_info(char *buf, int len)
{
    int offset = 0;
    offset += snprintf(buf + offset, len - offset, 
        "Architecture: RISC-V 64-bit\n");
    offset += snprintf(buf + offset, len - offset, 
        "CPU(s): %d\n", NCPU);
    offset += snprintf(buf + offset, len - offset, 
        "Model: QEMU RISC-V VirtIO Board\n");
    offset += snprintf(buf + offset, len - offset, 
        "Features: rv64imafdc\n");
    return offset;
}

// 读取进程列表
static int
read_proc_list(char *buf, int len)
{
    int offset = 0;
    offset += snprintf(buf + offset, len - offset, 
        "PID\tSTATE\t\tNAME\n");
    offset += snprintf(buf + offset, len - offset, 
        "---\t-----\t\t----\n");
    
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != UNUSED) {
            char *state;
            switch(p->state) {
                case SLEEPING: state = "SLEEPING"; break;
                case RUNNABLE: state = "RUNNABLE"; break;
                case RUNNING:  state = "RUNNING "; break;
                case ZOMBIE:   state = "ZOMBIE  "; break;
                default:       state = "UNKNOWN "; break;
            }
            offset += snprintf(buf + offset, len - offset,
                "%d\t%s\t%s\n", p->pid, state, p->name);
        }
        release(&p->lock);
        
        if (offset >= len - 64)
            break;
    }
    
    return offset;
}

// ============ sysfs 初始化 ============

static void
sysfs_add_node(struct sysfs_node *parent, struct sysfs_node *node)
{
    node->parent = parent;
    node->next = parent->children;
    parent->children = node;
}

void
sysfs_init(void)
{
    initlock(&sysfs_lock, "sysfs");
    
    // 初始化根目录
    strncpy(sysfs_root.name, "sys", sizeof(sysfs_root.name));
    sysfs_root.type = SYSFS_DIR;
    sysfs_root.mode = 0555;
    sysfs_root.children = 0;
    sysfs_root.parent = 0;
    sysfs_root.next = 0;
    
    // /sys/kernel 目录
    strncpy(sysfs_kernel.name, "kernel", sizeof(sysfs_kernel.name));
    sysfs_kernel.type = SYSFS_DIR;
    sysfs_kernel.mode = 0555;
    sysfs_kernel.children = 0;
    sysfs_add_node(&sysfs_root, &sysfs_kernel);
    
    // /sys/kernel/version
    strncpy(sysfs_kernel_version.name, "version", sizeof(sysfs_kernel_version.name));
    sysfs_kernel_version.type = SYSFS_FILE;
    sysfs_kernel_version.mode = 0444;
    sysfs_kernel_version.read = read_kernel_version;
    sysfs_add_node(&sysfs_kernel, &sysfs_kernel_version);
    
    // /sys/kernel/hostname
    strncpy(sysfs_kernel_hostname.name, "hostname", sizeof(sysfs_kernel_hostname.name));
    sysfs_kernel_hostname.type = SYSFS_FILE;
    sysfs_kernel_hostname.mode = 0644;
    sysfs_kernel_hostname.read = read_kernel_hostname;
    sysfs_add_node(&sysfs_kernel, &sysfs_kernel_hostname);
    
    // /sys/kernel/uptime
    strncpy(sysfs_kernel_uptime.name, "uptime", sizeof(sysfs_kernel_uptime.name));
    sysfs_kernel_uptime.type = SYSFS_FILE;
    sysfs_kernel_uptime.mode = 0444;
    sysfs_kernel_uptime.read = read_kernel_uptime;
    sysfs_add_node(&sysfs_kernel, &sysfs_kernel_uptime);
    
    // /sys/mem 目录
    strncpy(sysfs_mem.name, "mem", sizeof(sysfs_mem.name));
    sysfs_mem.type = SYSFS_DIR;
    sysfs_mem.mode = 0555;
    sysfs_mem.children = 0;
    sysfs_add_node(&sysfs_root, &sysfs_mem);
    
    // /sys/mem/total
    strncpy(sysfs_mem_total.name, "total", sizeof(sysfs_mem_total.name));
    sysfs_mem_total.type = SYSFS_FILE;
    sysfs_mem_total.mode = 0444;
    sysfs_mem_total.read = read_mem_total;
    sysfs_add_node(&sysfs_mem, &sysfs_mem_total);
    
    // /sys/mem/free
    strncpy(sysfs_mem_free.name, "free", sizeof(sysfs_mem_free.name));
    sysfs_mem_free.type = SYSFS_FILE;
    sysfs_mem_free.mode = 0444;
    sysfs_mem_free.read = read_mem_free;
    sysfs_add_node(&sysfs_mem, &sysfs_mem_free);
    
    // /sys/mem/used
    strncpy(sysfs_mem_used.name, "used", sizeof(sysfs_mem_used.name));
    sysfs_mem_used.type = SYSFS_FILE;
    sysfs_mem_used.mode = 0444;
    sysfs_mem_used.read = read_mem_used;
    sysfs_add_node(&sysfs_mem, &sysfs_mem_used);
    
    // /sys/cpu 目录
    strncpy(sysfs_cpu.name, "cpu", sizeof(sysfs_cpu.name));
    sysfs_cpu.type = SYSFS_DIR;
    sysfs_cpu.mode = 0555;
    sysfs_cpu.children = 0;
    sysfs_add_node(&sysfs_root, &sysfs_cpu);
    
    // /sys/cpu/count
    strncpy(sysfs_cpu_count.name, "count", sizeof(sysfs_cpu_count.name));
    sysfs_cpu_count.type = SYSFS_FILE;
    sysfs_cpu_count.mode = 0444;
    sysfs_cpu_count.read = read_cpu_count;
    sysfs_add_node(&sysfs_cpu, &sysfs_cpu_count);
    
    // /sys/cpu/info
    strncpy(sysfs_cpu_info.name, "info", sizeof(sysfs_cpu_info.name));
    sysfs_cpu_info.type = SYSFS_FILE;
    sysfs_cpu_info.mode = 0444;
    sysfs_cpu_info.read = read_cpu_info;
    sysfs_add_node(&sysfs_cpu, &sysfs_cpu_info);
    
    // /sys/proc 目录
    strncpy(sysfs_proc.name, "proc", sizeof(sysfs_proc.name));
    sysfs_proc.type = SYSFS_FILE;  // 特殊：动态生成内容
    sysfs_proc.mode = 0444;
    sysfs_proc.read = read_proc_list;
    sysfs_add_node(&sysfs_root, &sysfs_proc);
    
    printf("sysfs: initialized at /sys\n");
}

// ============ sysfs 查找和读取 ============

// 根据路径查找节点
static struct sysfs_node*
sysfs_lookup(char *path)
{
    if (path[0] == '/')
        path++;
    
    // 跳过 "sys/" 前缀
    if (strncmp(path, "sys/", 4) == 0)
        path += 4;
    else if (str_equal(path, "sys"))
        return &sysfs_root;
    
    struct sysfs_node *node = &sysfs_root;
    char name[32];
    
    while (*path) {
        // 提取下一个路径组件
        int i = 0;
        while (*path && *path != '/' && i < 31) {
            name[i++] = *path++;
        }
        name[i] = 0;
        
        if (*path == '/')
            path++;
        
        if (i == 0)
            continue;
        
        // 在当前节点的子节点中查找
        struct sysfs_node *child = node->children;
        while (child) {
            if (str_equal(child->name, name))
                break;
            child = child->next;
        }
        
        if (child == 0)
            return 0;  // 未找到
        
        node = child;
    }
    
    return node;
}

// 读取 sysfs 文件
int
sysfs_read(char *path, char *buf, int len)
{
    acquire(&sysfs_lock);
    
    struct sysfs_node *node = sysfs_lookup(path);
    if (node == 0) {
        release(&sysfs_lock);
        return -1;
    }
    
    if (node->type == SYSFS_DIR) {
        // 列出目录内容
        int offset = 0;
        struct sysfs_node *child = node->children;
        while (child && offset < len - 32) {
            offset += snprintf(buf + offset, len - offset, "%s%s\n",
                child->name, child->type == SYSFS_DIR ? "/" : "");
            child = child->next;
        }
        release(&sysfs_lock);
        return offset;
    }
    
    if (node->read == 0) {
        release(&sysfs_lock);
        return -1;
    }
    
    int n = node->read(buf, len);
    release(&sysfs_lock);
    return n;
}

// 列出 sysfs 目录
int
sysfs_list(char *path, char *buf, int len)
{
    acquire(&sysfs_lock);
    
    struct sysfs_node *node = sysfs_lookup(path);
    if (node == 0 || node->type != SYSFS_DIR) {
        release(&sysfs_lock);
        return -1;
    }
    
    int offset = 0;
    struct sysfs_node *child = node->children;
    while (child && offset < len - 32) {
        offset += snprintf(buf + offset, len - offset, "%s%s\n",
            child->name, child->type == SYSFS_DIR ? "/" : "");
        child = child->next;
    }
    
    release(&sysfs_lock);
    return offset;
}

// 检查路径是否是 sysfs 路径
int
sysfs_is_sysfs_path(char *path)
{
    if (path[0] == '/')
        path++;
    return strncmp(path, "sys", 3) == 0 && 
           (path[3] == '/' || path[3] == '\0');
}
