// versioning.c - 文件版本历史系统
// 支持文件版本控制、历史记录和回滚

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

extern uint ticks;

// ============ 常量定义 ============

#define MAX_VERSIONED_FILES 64
#define MAX_VERSIONS_PER_FILE 16
#define VERSION_PATH_LEN 128
#define VERSION_COMMENT_LEN 64

// 版本操作类型
#define VER_OP_CREATE   1
#define VER_OP_MODIFY   2
#define VER_OP_DELETE   3
#define VER_OP_RENAME   4
#define VER_OP_RESTORE  5

// ============ 数据结构 ============

// 单个版本记录
struct file_version {
    int used;
    int version_num;            // 版本号
    uint64 timestamp;           // 创建时间
    uint64 size;                // 文件大小
    uint32 checksum;            // 内容校验和
    int operation;              // 操作类型
    char comment[VERSION_COMMENT_LEN];  // 版本注释
    int creator_pid;            // 创建者进程ID
    
    // 差异存储 (delta storage)
    int is_full;                // 是否完整存储
    uint64 delta_base;          // 差异基准版本
    uint64 delta_size;          // 差异数据大小
};

// 版本化文件
struct versioned_file {
    int used;
    char path[VERSION_PATH_LEN];
    int file_id;                // 文件标识
    
    // 版本历史
    struct file_version versions[MAX_VERSIONS_PER_FILE];
    int version_count;
    int current_version;        // 当前版本号
    int head_version;           // 最新版本号
    
    // 配置
    int max_versions;           // 最大保留版本数
    int auto_version;           // 是否自动创建版本
    int version_on_close;       // 关闭时创建版本
    
    // 统计
    uint64 total_versions;
    uint64 total_restores;
    uint64 bytes_saved;         // 通过差异存储节省的空间
};

// ============ 全局状态 ============

struct version_manager {
    struct spinlock lock;
    struct versioned_file files[MAX_VERSIONED_FILES];
    int file_count;
    int next_file_id;
    
    // 全局配置
    int enabled;
    int default_max_versions;
    int auto_cleanup;           // 自动清理旧版本
    
    // 统计
    uint64 total_versions_created;
    uint64 total_restores;
    uint64 total_cleanups;
    uint64 storage_used;
};

static struct version_manager ver_mgr;

// ============ 初始化 ============

void
versioning_init(void)
{
    initlock(&ver_mgr.lock, "versioning");
    memset(ver_mgr.files, 0, sizeof(ver_mgr.files));
    ver_mgr.file_count = 0;
    ver_mgr.next_file_id = 1;
    ver_mgr.enabled = 1;
    ver_mgr.default_max_versions = 10;
    ver_mgr.auto_cleanup = 1;
    ver_mgr.total_versions_created = 0;
    ver_mgr.total_restores = 0;
    ver_mgr.total_cleanups = 0;
    ver_mgr.storage_used = 0;
    
    printf("versioning: file version history initialized\n");
}

// ============ 辅助函数 ============

// 计算简单校验和
static uint32
compute_file_checksum(uint64 size)
{
    // 简化实现
    return (uint32)(size * 31 + 17);
}

// 查找版本化文件
static struct versioned_file*
find_versioned_file(char *path)
{
    for (int i = 0; i < MAX_VERSIONED_FILES; i++) {
        if (ver_mgr.files[i].used &&
            strncmp(ver_mgr.files[i].path, path, VERSION_PATH_LEN) == 0) {
            return &ver_mgr.files[i];
        }
    }
    return 0;
}

// ============ 版本控制接口 ============

// 启用文件版本控制
int
version_enable(char *path)
{
    acquire(&ver_mgr.lock);
    
    // 检查是否已启用
    if (find_versioned_file(path)) {
        release(&ver_mgr.lock);
        return 0;  // 已启用
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_VERSIONED_FILES; i++) {
        if (!ver_mgr.files[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    struct versioned_file *vf = &ver_mgr.files[slot];
    memset(vf, 0, sizeof(struct versioned_file));
    vf->used = 1;
    strncpy(vf->path, path, VERSION_PATH_LEN);
    vf->file_id = ver_mgr.next_file_id++;
    vf->version_count = 0;
    vf->current_version = 0;
    vf->head_version = 0;
    vf->max_versions = ver_mgr.default_max_versions;
    vf->auto_version = 1;
    vf->version_on_close = 0;
    
    ver_mgr.file_count++;
    
    release(&ver_mgr.lock);
    
    printf("versioning: enabled for %s\n", path);
    return vf->file_id;
}

// 禁用文件版本控制
int
version_disable(char *path)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    vf->used = 0;
    ver_mgr.file_count--;
    
    release(&ver_mgr.lock);
    return 0;
}

// 创建新版本
int
version_create(char *path, char *comment)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    // 检查版本数量限制
    if (vf->version_count >= MAX_VERSIONS_PER_FILE) {
        // 清理最旧的版本
        if (ver_mgr.auto_cleanup) {
            for (int i = 0; i < MAX_VERSIONS_PER_FILE - 1; i++) {
                vf->versions[i] = vf->versions[i + 1];
            }
            vf->version_count--;
            ver_mgr.total_cleanups++;
        } else {
            release(&ver_mgr.lock);
            return -1;
        }
    }
    
    // 创建新版本
    int idx = vf->version_count;
    struct file_version *ver = &vf->versions[idx];
    
    ver->used = 1;
    ver->version_num = ++vf->head_version;
    ver->timestamp = ticks;
    ver->size = 0;  // 实际实现需要获取文件大小
    ver->checksum = compute_file_checksum(ver->size);
    ver->operation = VER_OP_MODIFY;
    if (comment) {
        strncpy(ver->comment, comment, VERSION_COMMENT_LEN);
    } else {
        strncpy(ver->comment, "auto", VERSION_COMMENT_LEN);
    }
    ver->creator_pid = myproc()->pid;
    ver->is_full = 1;
    ver->delta_base = 0;
    ver->delta_size = 0;
    
    vf->version_count++;
    vf->current_version = ver->version_num;
    vf->total_versions++;
    
    ver_mgr.total_versions_created++;
    
    release(&ver_mgr.lock);
    
    return ver->version_num;
}

// 恢复到指定版本
int
version_restore(char *path, int version_num)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    // 查找版本
    struct file_version *ver = 0;
    for (int i = 0; i < vf->version_count; i++) {
        if (vf->versions[i].used && vf->versions[i].version_num == version_num) {
            ver = &vf->versions[i];
            break;
        }
    }
    
    if (!ver) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    // 恢复版本 (简化实现)
    vf->current_version = version_num;
    vf->total_restores++;
    ver_mgr.total_restores++;
    
    // 创建恢复记录
    if (vf->version_count < MAX_VERSIONS_PER_FILE) {
        int idx = vf->version_count++;
        struct file_version *restore_ver = &vf->versions[idx];
        restore_ver->used = 1;
        restore_ver->version_num = ++vf->head_version;
        restore_ver->timestamp = ticks;
        restore_ver->operation = VER_OP_RESTORE;
        snprintf(restore_ver->comment, VERSION_COMMENT_LEN, 
                 "restored from v%d", version_num);
        restore_ver->creator_pid = myproc()->pid;
        vf->current_version = restore_ver->version_num;
    }
    
    release(&ver_mgr.lock);
    
    printf("versioning: restored %s to version %d\n", path, version_num);
    return 0;
}

// 获取版本列表
int
version_list(char *path, char *buf, int len)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return snprintf(buf, len, "File not versioned: %s\n", path);
    }
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "Version history for %s:\n", path);
    offset += snprintf(buf + offset, len - offset,
        "VER\tTIME\t\tSIZE\tOP\tCOMMENT\n");
    
    char *op_names[] = {"", "CREATE", "MODIFY", "DELETE", "RENAME", "RESTORE"};
    
    for (int i = 0; i < vf->version_count && offset < len - 80; i++) {
        struct file_version *ver = &vf->versions[i];
        if (ver->used) {
            char marker = (ver->version_num == vf->current_version) ? '*' : ' ';
            offset += snprintf(buf + offset, len - offset,
                "%c%d\t%d\t\t%d\t%s\t%s\n",
                marker, ver->version_num, (int)ver->timestamp,
                (int)ver->size, op_names[ver->operation], ver->comment);
        }
    }
    
    release(&ver_mgr.lock);
    return offset;
}

// 获取版本信息
int
version_info(char *path, int version_num, uint64 *timestamp, uint64 *size, char *comment)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    for (int i = 0; i < vf->version_count; i++) {
        struct file_version *ver = &vf->versions[i];
        if (ver->used && ver->version_num == version_num) {
            if (timestamp) *timestamp = ver->timestamp;
            if (size) *size = ver->size;
            if (comment) strncpy(comment, ver->comment, VERSION_COMMENT_LEN);
            release(&ver_mgr.lock);
            return 0;
        }
    }
    
    release(&ver_mgr.lock);
    return -1;
}

// 删除指定版本
int
version_delete(char *path, int version_num)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    // 不能删除当前版本
    if (version_num == vf->current_version) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    for (int i = 0; i < vf->version_count; i++) {
        if (vf->versions[i].used && vf->versions[i].version_num == version_num) {
            // 移除版本
            for (int j = i; j < vf->version_count - 1; j++) {
                vf->versions[j] = vf->versions[j + 1];
            }
            vf->version_count--;
            ver_mgr.total_cleanups++;
            release(&ver_mgr.lock);
            return 0;
        }
    }
    
    release(&ver_mgr.lock);
    return -1;
}

// 清理旧版本
int
version_cleanup(char *path, int keep_count)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    int deleted = 0;
    while (vf->version_count > keep_count) {
        // 删除最旧的版本 (保留当前版本)
        int oldest_idx = -1;
        int oldest_ver = 0x7FFFFFFF;
        
        for (int i = 0; i < vf->version_count; i++) {
            if (vf->versions[i].used && 
                vf->versions[i].version_num != vf->current_version &&
                vf->versions[i].version_num < oldest_ver) {
                oldest_ver = vf->versions[i].version_num;
                oldest_idx = i;
            }
        }
        
        if (oldest_idx < 0) break;
        
        // 移除
        for (int j = oldest_idx; j < vf->version_count - 1; j++) {
            vf->versions[j] = vf->versions[j + 1];
        }
        vf->version_count--;
        deleted++;
    }
    
    ver_mgr.total_cleanups += deleted;
    
    release(&ver_mgr.lock);
    return deleted;
}

// 比较两个版本
int
version_diff(char *path, int ver1, int ver2, char *buf, int len)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return snprintf(buf, len, "File not versioned\n");
    }
    
    struct file_version *v1 = 0, *v2 = 0;
    for (int i = 0; i < vf->version_count; i++) {
        if (vf->versions[i].version_num == ver1) v1 = &vf->versions[i];
        if (vf->versions[i].version_num == ver2) v2 = &vf->versions[i];
    }
    
    if (!v1 || !v2) {
        release(&ver_mgr.lock);
        return snprintf(buf, len, "Version not found\n");
    }
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "Diff between v%d and v%d:\n", ver1, ver2);
    offset += snprintf(buf + offset, len - offset,
        "  Size: %d -> %d (%+d bytes)\n",
        (int)v1->size, (int)v2->size, (int)(v2->size - v1->size));
    offset += snprintf(buf + offset, len - offset,
        "  Time: %d -> %d\n",
        (int)v1->timestamp, (int)v2->timestamp);
    
    release(&ver_mgr.lock);
    return offset;
}

// 获取当前版本号
int
version_current(char *path)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    int ver = vf->current_version;
    release(&ver_mgr.lock);
    return ver;
}

// 设置版本配置
int
version_set_config(char *path, int max_versions, int auto_version)
{
    acquire(&ver_mgr.lock);
    
    struct versioned_file *vf = find_versioned_file(path);
    if (!vf) {
        release(&ver_mgr.lock);
        return -1;
    }
    
    if (max_versions > 0) vf->max_versions = max_versions;
    if (auto_version >= 0) vf->auto_version = auto_version;
    
    release(&ver_mgr.lock);
    return 0;
}

// ============ 统计信息 ============

void
versioning_print_stats(void)
{
    acquire(&ver_mgr.lock);
    
    printf("\n=== File Versioning Statistics ===\n");
    printf("Versioned files: %d\n", ver_mgr.file_count);
    printf("Total versions created: %d\n", (int)ver_mgr.total_versions_created);
    printf("Total restores: %d\n", (int)ver_mgr.total_restores);
    printf("Total cleanups: %d\n", (int)ver_mgr.total_cleanups);
    printf("Storage used: %d KB\n", (int)(ver_mgr.storage_used / 1024));
    printf("==================================\n");
    
    release(&ver_mgr.lock);
}

void
versioning_get_stats(uint64 *versions, uint64 *restores, uint64 *cleanups)
{
    acquire(&ver_mgr.lock);
    if (versions) *versions = ver_mgr.total_versions_created;
    if (restores) *restores = ver_mgr.total_restores;
    if (cleanups) *cleanups = ver_mgr.total_cleanups;
    release(&ver_mgr.lock);
}

// 列出所有版本化文件
int
versioning_list_files(char *buf, int len)
{
    acquire(&ver_mgr.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "Versioned files:\n");
    offset += snprintf(buf + offset, len - offset,
        "PATH\t\t\t\tVERSIONS\tCURRENT\n");
    
    for (int i = 0; i < MAX_VERSIONED_FILES && offset < len - 80; i++) {
        struct versioned_file *vf = &ver_mgr.files[i];
        if (vf->used) {
            offset += snprintf(buf + offset, len - offset,
                "%s\t\t%d\t\tv%d\n",
                vf->path, vf->version_count, vf->current_version);
        }
    }
    
    release(&ver_mgr.lock);
    return offset;
}
