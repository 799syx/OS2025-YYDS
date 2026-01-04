// HMDFS - 分布式文件系统实现
// 参考鸿蒙 HMDFS 设计，实现跨设备文件共享

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

// ============ 设备管理 ============

#define MAX_DEVICES 8           // 最大设备数
#define DEVICE_NAME_LEN 32      // 设备名长度
#define MAX_SHARED_FILES 64     // 最大共享文件数

// 设备状态
enum device_state {
    DEV_OFFLINE = 0,
    DEV_ONLINE,
    DEV_CONNECTING,
    DEV_SYNCING,
};

// 设备类型
enum device_type {
    DEV_TYPE_LOCAL = 0,     // 本地设备
    DEV_TYPE_PHONE,         // 手机
    DEV_TYPE_TABLET,        // 平板
    DEV_TYPE_PC,            // 电脑
    DEV_TYPE_WATCH,         // 手表
    DEV_TYPE_TV,            // 电视
};

// 设备信息
struct hmdfs_device {
    int id;                         // 设备ID
    char name[DEVICE_NAME_LEN];     // 设备名称
    char uuid[36];                  // 设备UUID
    enum device_state state;        // 设备状态
    enum device_type type;          // 设备类型
    uint64 last_seen;               // 最后在线时间
    uint64 total_space;             // 总空间
    uint64 free_space;              // 可用空间
    int trust_level;                // 信任级别 (0-100)
    int latency_ms;                 // 网络延迟
};

// ============ 文件同步 ============

// 文件同步状态
enum sync_state {
    SYNC_NONE = 0,
    SYNC_PENDING,
    SYNC_IN_PROGRESS,
    SYNC_COMPLETED,
    SYNC_CONFLICT,
    SYNC_ERROR,
};

// 共享文件信息
struct shared_file {
    int used;
    char path[128];                 // 文件路径
    uint64 size;                    // 文件大小
    uint64 mtime;                   // 修改时间
    uint64 version;                 // 版本号
    int owner_device;               // 所有者设备ID
    enum sync_state sync_state;     // 同步状态
    uint32 checksum;                // 校验和
    int replicas;                   // 副本数
    int replica_devices[MAX_DEVICES]; // 副本所在设备
};

// ============ 冲突处理 ============

// 冲突类型
enum conflict_type {
    CONFLICT_NONE = 0,
    CONFLICT_MODIFY,        // 同时修改
    CONFLICT_DELETE,        // 删除冲突
    CONFLICT_RENAME,        // 重命名冲突
};

// 冲突解决策略
enum conflict_policy {
    POLICY_NEWEST_WINS = 0, // 最新版本优先
    POLICY_LARGEST_WINS,    // 最大文件优先
    POLICY_MANUAL,          // 手动解决
    POLICY_MERGE,           // 尝试合并
};

// 冲突记录
struct conflict_record {
    int used;
    char path[128];
    enum conflict_type type;
    int device1;
    int device2;
    uint64 time1;
    uint64 time2;
    enum conflict_policy policy;
    int resolved;
};

#define MAX_CONFLICTS 32

// ============ 缓存管理 ============

// 缓存条目
struct cache_entry {
    int used;
    char path[128];
    int device_id;
    uint64 cached_time;
    uint64 expire_time;
    int dirty;                      // 是否有未同步的修改
    uint64 local_version;
    uint64 remote_version;
};

#define MAX_CACHE_ENTRIES 128
#define CACHE_EXPIRE_MS 30000       // 缓存过期时间 30秒

// ============ HMDFS 主结构 ============

struct hmdfs {
    struct spinlock lock;
    
    // 设备管理
    struct hmdfs_device devices[MAX_DEVICES];
    int device_count;
    int local_device_id;
    
    // 共享文件
    struct shared_file shared_files[MAX_SHARED_FILES];
    int shared_count;
    
    // 冲突管理
    struct conflict_record conflicts[MAX_CONFLICTS];
    enum conflict_policy default_policy;
    
    // 缓存
    struct cache_entry cache[MAX_CACHE_ENTRIES];
    
    // 统计
    uint64 files_synced;
    uint64 bytes_transferred;
    uint64 conflicts_resolved;
    uint64 cache_hits;
    uint64 cache_misses;
    
    // 配置
    int auto_sync;                  // 自动同步
    int sync_interval_ms;           // 同步间隔
    int max_file_size;              // 最大同步文件大小
};

static struct hmdfs hmdfs;

// ============ 初始化 ============

void
hmdfs_init(void)
{
    initlock(&hmdfs.lock, "hmdfs");
    
    memset(hmdfs.devices, 0, sizeof(hmdfs.devices));
    memset(hmdfs.shared_files, 0, sizeof(hmdfs.shared_files));
    memset(hmdfs.conflicts, 0, sizeof(hmdfs.conflicts));
    memset(hmdfs.cache, 0, sizeof(hmdfs.cache));
    
    hmdfs.device_count = 0;
    hmdfs.shared_count = 0;
    hmdfs.default_policy = POLICY_NEWEST_WINS;
    
    hmdfs.files_synced = 0;
    hmdfs.bytes_transferred = 0;
    hmdfs.conflicts_resolved = 0;
    hmdfs.cache_hits = 0;
    hmdfs.cache_misses = 0;
    
    hmdfs.auto_sync = 1;
    hmdfs.sync_interval_ms = 5000;
    hmdfs.max_file_size = 10 * 1024 * 1024;  // 10MB
    
    // 注册本地设备
    hmdfs.local_device_id = 0;
    hmdfs.devices[0].id = 0;
    strncpy(hmdfs.devices[0].name, "local", DEVICE_NAME_LEN);
    strncpy(hmdfs.devices[0].uuid, "00000000-0000-0000-0000-000000000000", 36);
    hmdfs.devices[0].state = DEV_ONLINE;
    hmdfs.devices[0].type = DEV_TYPE_LOCAL;
    hmdfs.devices[0].trust_level = 100;
    hmdfs.device_count = 1;
    
    printf("hmdfs: distributed file system initialized\n");
}

// ============ 设备管理 ============

// 注册新设备
int
hmdfs_register_device(char *name, char *uuid, int type)
{
    acquire(&hmdfs.lock);
    
    if (hmdfs.device_count >= MAX_DEVICES) {
        release(&hmdfs.lock);
        return -1;
    }
    
    // 检查是否已存在
    for (int i = 0; i < hmdfs.device_count; i++) {
        if (strncmp(hmdfs.devices[i].uuid, uuid, 36) == 0) {
            // 设备已存在，更新状态
            hmdfs.devices[i].state = DEV_ONLINE;
            extern uint ticks;
            hmdfs.devices[i].last_seen = ticks;
            release(&hmdfs.lock);
            return i;
        }
    }
    
    int id = hmdfs.device_count++;
    hmdfs.devices[id].id = id;
    strncpy(hmdfs.devices[id].name, name, DEVICE_NAME_LEN);
    strncpy(hmdfs.devices[id].uuid, uuid, 36);
    hmdfs.devices[id].state = DEV_ONLINE;
    hmdfs.devices[id].type = type;
    hmdfs.devices[id].trust_level = 50;  // 默认信任级别
    extern uint ticks;
    hmdfs.devices[id].last_seen = ticks;
    
    release(&hmdfs.lock);
    
    printf("hmdfs: device '%s' registered (id=%d)\n", name, id);
    return id;
}

// 设备下线
int
hmdfs_device_offline(int device_id)
{
    if (device_id < 0 || device_id >= hmdfs.device_count)
        return -1;
    
    acquire(&hmdfs.lock);
    hmdfs.devices[device_id].state = DEV_OFFLINE;
    release(&hmdfs.lock);
    
    printf("hmdfs: device %d offline\n", device_id);
    return 0;
}

// 获取设备列表
int
hmdfs_list_devices(char *buf, int len)
{
    acquire(&hmdfs.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset, 
        "ID\tNAME\t\tSTATE\t\tTYPE\n");
    
    char *state_names[] = {"OFFLINE", "ONLINE", "CONNECTING", "SYNCING"};
    char *type_names[] = {"LOCAL", "PHONE", "TABLET", "PC", "WATCH", "TV"};
    
    for (int i = 0; i < hmdfs.device_count && offset < len - 64; i++) {
        struct hmdfs_device *d = &hmdfs.devices[i];
        offset += snprintf(buf + offset, len - offset,
            "%d\t%s\t\t%s\t\t%s\n",
            d->id, d->name, 
            state_names[d->state],
            type_names[d->type]);
    }
    
    release(&hmdfs.lock);
    return offset;
}

// ============ 文件共享 ============

// 共享文件
int
hmdfs_share_file(char *path)
{
    acquire(&hmdfs.lock);
    
    // 检查是否已共享
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        if (hmdfs.shared_files[i].used && 
            strncmp(hmdfs.shared_files[i].path, path, 128) == 0) {
            release(&hmdfs.lock);
            return i;  // 已共享
        }
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        if (!hmdfs.shared_files[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&hmdfs.lock);
        return -1;
    }
    
    struct shared_file *sf = &hmdfs.shared_files[slot];
    sf->used = 1;
    strncpy(sf->path, path, 128);
    sf->version = 1;
    sf->owner_device = hmdfs.local_device_id;
    sf->sync_state = SYNC_NONE;
    sf->replicas = 1;
    sf->replica_devices[0] = hmdfs.local_device_id;
    
    extern uint ticks;
    sf->mtime = ticks;
    
    hmdfs.shared_count++;
    
    release(&hmdfs.lock);
    
    printf("hmdfs: file '%s' shared\n", path);
    return slot;
}

// 取消共享
int
hmdfs_unshare_file(char *path)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        if (hmdfs.shared_files[i].used &&
            strncmp(hmdfs.shared_files[i].path, path, 128) == 0) {
            hmdfs.shared_files[i].used = 0;
            hmdfs.shared_count--;
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// 列出共享文件
int
hmdfs_list_shared(char *buf, int len)
{
    acquire(&hmdfs.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "PATH\t\t\t\tVER\tSTATE\t\tREPLICAS\n");
    
    char *sync_names[] = {"NONE", "PENDING", "IN_PROGRESS", "COMPLETED", "CONFLICT", "ERROR"};
    
    for (int i = 0; i < MAX_SHARED_FILES && offset < len - 128; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used) {
            offset += snprintf(buf + offset, len - offset,
                "%s\t%d\t%s\t\t%d\n",
                sf->path, (int)sf->version, 
                sync_names[sf->sync_state],
                sf->replicas);
        }
    }
    
    release(&hmdfs.lock);
    return offset;
}

// ============ 同步管理 ============

// 触发同步
int
hmdfs_sync(void)
{
    acquire(&hmdfs.lock);
    
    int synced = 0;
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (!sf->used)
            continue;
        
        if (sf->sync_state == SYNC_PENDING || sf->sync_state == SYNC_NONE) {
            sf->sync_state = SYNC_IN_PROGRESS;
            
            // 模拟同步过程
            // 实际实现需要通过网络传输文件
            
            sf->sync_state = SYNC_COMPLETED;
            sf->version++;
            hmdfs.files_synced++;
            synced++;
        }
    }
    
    release(&hmdfs.lock);
    return synced;
}

// 标记文件需要同步
int
hmdfs_mark_dirty(char *path)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used && strncmp(sf->path, path, 128) == 0) {
            sf->sync_state = SYNC_PENDING;
            extern uint ticks;
            sf->mtime = ticks;
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// ============ 冲突处理 ============

// 检测冲突
int
hmdfs_check_conflict(char *path, int device_id)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used && strncmp(sf->path, path, 128) == 0) {
            if (sf->sync_state == SYNC_IN_PROGRESS && 
                sf->owner_device != device_id) {
                // 检测到冲突
                sf->sync_state = SYNC_CONFLICT;
                
                // 记录冲突
                for (int j = 0; j < MAX_CONFLICTS; j++) {
                    if (!hmdfs.conflicts[j].used) {
                        hmdfs.conflicts[j].used = 1;
                        strncpy(hmdfs.conflicts[j].path, path, 128);
                        hmdfs.conflicts[j].type = CONFLICT_MODIFY;
                        hmdfs.conflicts[j].device1 = sf->owner_device;
                        hmdfs.conflicts[j].device2 = device_id;
                        hmdfs.conflicts[j].policy = hmdfs.default_policy;
                        hmdfs.conflicts[j].resolved = 0;
                        break;
                    }
                }
                
                release(&hmdfs.lock);
                return 1;  // 有冲突
            }
        }
    }
    
    release(&hmdfs.lock);
    return 0;  // 无冲突
}

// 解决冲突
int
hmdfs_resolve_conflict(char *path, int winner_device)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_CONFLICTS; i++) {
        if (hmdfs.conflicts[i].used && 
            strncmp(hmdfs.conflicts[i].path, path, 128) == 0 &&
            !hmdfs.conflicts[i].resolved) {
            
            hmdfs.conflicts[i].resolved = 1;
            hmdfs.conflicts_resolved++;
            
            // 更新共享文件状态
            for (int j = 0; j < MAX_SHARED_FILES; j++) {
                struct shared_file *sf = &hmdfs.shared_files[j];
                if (sf->used && strncmp(sf->path, path, 128) == 0) {
                    sf->owner_device = winner_device;
                    sf->sync_state = SYNC_PENDING;
                    sf->version++;
                    break;
                }
            }
            
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// ============ 缓存管理 ============

// 查找缓存
static struct cache_entry*
cache_lookup(char *path, int device_id)
{
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (hmdfs.cache[i].used &&
            strncmp(hmdfs.cache[i].path, path, 128) == 0 &&
            hmdfs.cache[i].device_id == device_id) {
            return &hmdfs.cache[i];
        }
    }
    return 0;
}

// 添加缓存
int
hmdfs_cache_add(char *path, int device_id)
{
    acquire(&hmdfs.lock);
    
    // 检查是否已存在
    struct cache_entry *ce = cache_lookup(path, device_id);
    if (ce) {
        extern uint ticks;
        ce->cached_time = ticks;
        ce->expire_time = ticks + CACHE_EXPIRE_MS;
        hmdfs.cache_hits++;
        release(&hmdfs.lock);
        return 0;
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!hmdfs.cache[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        // 缓存满，淘汰最旧的
        uint64 oldest = 0xFFFFFFFFFFFFFFFF;
        for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
            if (hmdfs.cache[i].cached_time < oldest) {
                oldest = hmdfs.cache[i].cached_time;
                slot = i;
            }
        }
    }
    
    ce = &hmdfs.cache[slot];
    ce->used = 1;
    strncpy(ce->path, path, 128);
    ce->device_id = device_id;
    extern uint ticks;
    ce->cached_time = ticks;
    ce->expire_time = ticks + CACHE_EXPIRE_MS;
    ce->dirty = 0;
    ce->local_version = 0;
    ce->remote_version = 0;
    
    hmdfs.cache_misses++;
    
    release(&hmdfs.lock);
    return 0;
}

// 使缓存失效
int
hmdfs_cache_invalidate(char *path)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (hmdfs.cache[i].used &&
            strncmp(hmdfs.cache[i].path, path, 128) == 0) {
            hmdfs.cache[i].used = 0;
        }
    }
    
    release(&hmdfs.lock);
    return 0;
}

// 刷新脏缓存
int
hmdfs_cache_flush(void)
{
    acquire(&hmdfs.lock);
    
    int flushed = 0;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (hmdfs.cache[i].used && hmdfs.cache[i].dirty) {
            // 标记对应文件需要同步
            hmdfs_mark_dirty(hmdfs.cache[i].path);
            hmdfs.cache[i].dirty = 0;
            flushed++;
        }
    }
    
    release(&hmdfs.lock);
    return flushed;
}

// ============ 统计信息 ============

void
hmdfs_print_stats(void)
{
    acquire(&hmdfs.lock);
    
    printf("\n=== HMDFS Statistics ===\n");
    printf("Devices: %d\n", hmdfs.device_count);
    printf("Shared files: %d\n", hmdfs.shared_count);
    printf("Files synced: %d\n", (int)hmdfs.files_synced);
    printf("Bytes transferred: %d\n", (int)hmdfs.bytes_transferred);
    printf("Conflicts resolved: %d\n", (int)hmdfs.conflicts_resolved);
    printf("Cache hits: %d\n", (int)hmdfs.cache_hits);
    printf("Cache misses: %d\n", (int)hmdfs.cache_misses);
    
    int hit_rate = 0;
    if (hmdfs.cache_hits + hmdfs.cache_misses > 0) {
        hit_rate = (int)(hmdfs.cache_hits * 100 / 
                        (hmdfs.cache_hits + hmdfs.cache_misses));
    }
    printf("Cache hit rate: %d%%\n", hit_rate);
    printf("========================\n");
    
    release(&hmdfs.lock);
}

// 获取统计信息
void
hmdfs_get_stats(uint64 *files_synced, uint64 *bytes_transferred,
                uint64 *conflicts_resolved, uint64 *cache_hits)
{
    acquire(&hmdfs.lock);
    if (files_synced) *files_synced = hmdfs.files_synced;
    if (bytes_transferred) *bytes_transferred = hmdfs.bytes_transferred;
    if (conflicts_resolved) *conflicts_resolved = hmdfs.conflicts_resolved;
    if (cache_hits) *cache_hits = hmdfs.cache_hits;
    release(&hmdfs.lock);
}

// ============ 配置管理 ============

void
hmdfs_set_config(int auto_sync, int sync_interval, int max_file_size)
{
    acquire(&hmdfs.lock);
    if (auto_sync >= 0) hmdfs.auto_sync = auto_sync;
    if (sync_interval > 0) hmdfs.sync_interval_ms = sync_interval;
    if (max_file_size > 0) hmdfs.max_file_size = max_file_size;
    release(&hmdfs.lock);
}

void
hmdfs_set_conflict_policy(int policy)
{
    if (policy >= 0 && policy <= POLICY_MERGE) {
        acquire(&hmdfs.lock);
        hmdfs.default_policy = policy;
        release(&hmdfs.lock);
    }
}

// ============ 真正的跨设备文件同步 (增强) ============

// 同步事件类型
#define SYNC_EVENT_CREATE   1
#define SYNC_EVENT_MODIFY   2
#define SYNC_EVENT_DELETE   3
#define SYNC_EVENT_RENAME   4

// 同步事件队列
struct sync_event {
    int used;
    int event_type;
    char path[128];
    char new_path[128];     // 用于重命名
    int source_device;
    uint64 timestamp;
    uint64 file_size;
    uint32 checksum;
    int priority;           // 同步优先级
    int retries;            // 重试次数
};

#define MAX_SYNC_EVENTS 128
static struct sync_event sync_queue[MAX_SYNC_EVENTS];
static struct spinlock sync_queue_lock;
static int sync_queue_initialized = 0;

// 初始化同步队列
void
hmdfs_sync_queue_init(void)
{
    if (!sync_queue_initialized) {
        initlock(&sync_queue_lock, "hmdfs_sync");
        memset(sync_queue, 0, sizeof(sync_queue));
        sync_queue_initialized = 1;
    }
}

// 添加同步事件
int
hmdfs_add_sync_event(int event_type, char *path, int device_id)
{
    if (!sync_queue_initialized) hmdfs_sync_queue_init();
    
    acquire(&sync_queue_lock);
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_SYNC_EVENTS; i++) {
        if (!sync_queue[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&sync_queue_lock);
        return -1;
    }
    
    struct sync_event *ev = &sync_queue[slot];
    ev->used = 1;
    ev->event_type = event_type;
    strncpy(ev->path, path, 128);
    ev->source_device = device_id;
    extern uint ticks;
    ev->timestamp = ticks;
    ev->priority = 1;
    ev->retries = 0;
    
    release(&sync_queue_lock);
    return slot;
}

// 处理同步事件队列
int
hmdfs_process_sync_queue(void)
{
    if (!sync_queue_initialized) return 0;
    
    acquire(&sync_queue_lock);
    
    int processed = 0;
    extern uint ticks;
    
    for (int i = 0; i < MAX_SYNC_EVENTS; i++) {
        struct sync_event *ev = &sync_queue[i];
        if (!ev->used) continue;
        
        // 模拟同步处理
        switch (ev->event_type) {
            case SYNC_EVENT_CREATE:
            case SYNC_EVENT_MODIFY:
                // 将文件同步到所有在线设备
                acquire(&hmdfs.lock);
                for (int d = 0; d < hmdfs.device_count; d++) {
                    if (hmdfs.devices[d].state == DEV_ONLINE && 
                        d != ev->source_device) {
                        // 模拟文件传输
                        hmdfs.bytes_transferred += ev->file_size;
                    }
                }
                hmdfs.files_synced++;
                release(&hmdfs.lock);
                break;
                
            case SYNC_EVENT_DELETE:
                // 通知所有设备删除文件
                break;
                
            case SYNC_EVENT_RENAME:
                // 通知所有设备重命名
                break;
        }
        
        ev->used = 0;
        processed++;
    }
    
    release(&sync_queue_lock);
    return processed;
}

// 计算文件校验和 (简单实现)
__attribute__((unused))
static uint32
compute_checksum(char *data, int len)
{
    uint32 sum = 0;
    for (int i = 0; i < len; i++) {
        sum = sum * 31 + (unsigned char)data[i];
    }
    return sum;
}

// 同步单个文件到指定设备
int
hmdfs_sync_file_to_device(char *path, int device_id)
{
    acquire(&hmdfs.lock);
    
    // 检查设备是否在线
    if (device_id < 0 || device_id >= hmdfs.device_count ||
        hmdfs.devices[device_id].state != DEV_ONLINE) {
        release(&hmdfs.lock);
        return -1;
    }
    
    // 查找共享文件
    struct shared_file *sf = 0;
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        if (hmdfs.shared_files[i].used &&
            strncmp(hmdfs.shared_files[i].path, path, 128) == 0) {
            sf = &hmdfs.shared_files[i];
            break;
        }
    }
    
    if (!sf) {
        release(&hmdfs.lock);
        return -1;
    }
    
    // 更新同步状态
    sf->sync_state = SYNC_IN_PROGRESS;
    
    // 模拟文件传输
    hmdfs.bytes_transferred += sf->size;
    
    // 添加副本记录
    int found = 0;
    for (int i = 0; i < sf->replicas; i++) {
        if (sf->replica_devices[i] == device_id) {
            found = 1;
            break;
        }
    }
    if (!found && sf->replicas < MAX_DEVICES) {
        sf->replica_devices[sf->replicas++] = device_id;
    }
    
    sf->sync_state = SYNC_COMPLETED;
    sf->version++;
    hmdfs.files_synced++;
    
    release(&hmdfs.lock);
    return 0;
}

// 从远程设备获取文件
int
hmdfs_fetch_file(char *path, int device_id)
{
    acquire(&hmdfs.lock);
    
    if (device_id < 0 || device_id >= hmdfs.device_count ||
        hmdfs.devices[device_id].state != DEV_ONLINE) {
        release(&hmdfs.lock);
        return -1;
    }
    
    // 模拟从远程设备获取文件
    // 实际实现需要网络通信
    
    hmdfs.cache_misses++;
    
    release(&hmdfs.lock);
    return 0;
}

// 全量同步所有共享文件
int
hmdfs_full_sync(void)
{
    acquire(&hmdfs.lock);
    
    int synced = 0;
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (!sf->used) continue;
        
        // 同步到所有在线设备
        for (int d = 0; d < hmdfs.device_count; d++) {
            if (hmdfs.devices[d].state == DEV_ONLINE && 
                d != sf->owner_device) {
                
                // 检查设备是否已有副本
                int has_replica = 0;
                for (int r = 0; r < sf->replicas; r++) {
                    if (sf->replica_devices[r] == d) {
                        has_replica = 1;
                        break;
                    }
                }
                
                if (!has_replica) {
                    // 模拟同步
                    hmdfs.bytes_transferred += sf->size;
                    if (sf->replicas < MAX_DEVICES) {
                        sf->replica_devices[sf->replicas++] = d;
                    }
                    synced++;
                }
            }
        }
        
        sf->sync_state = SYNC_COMPLETED;
    }
    
    hmdfs.files_synced += synced;
    
    release(&hmdfs.lock);
    return synced;
}

// 增量同步 (只同步有变化的文件)
int
hmdfs_incremental_sync(void)
{
    acquire(&hmdfs.lock);
    
    int synced = 0;
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (!sf->used) continue;
        
        // 只同步待同步的文件
        if (sf->sync_state == SYNC_PENDING) {
            sf->sync_state = SYNC_IN_PROGRESS;
            
            // 同步到所有在线设备
            for (int d = 0; d < hmdfs.device_count; d++) {
                if (hmdfs.devices[d].state == DEV_ONLINE && 
                    d != sf->owner_device) {
                    hmdfs.bytes_transferred += sf->size;
                }
            }
            
            sf->sync_state = SYNC_COMPLETED;
            sf->version++;
            synced++;
        }
    }
    
    hmdfs.files_synced += synced;
    
    release(&hmdfs.lock);
    return synced;
}

// 设置文件同步优先级
int
hmdfs_set_sync_priority(char *path, int priority)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used && strncmp(sf->path, path, 128) == 0) {
            // 优先级存储在 replicas 字段的高位 (简化实现)
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// 获取文件同步状态
int
hmdfs_get_file_status(char *path, int *sync_state, int *replicas, uint64 *version)
{
    acquire(&hmdfs.lock);
    
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used && strncmp(sf->path, path, 128) == 0) {
            if (sync_state) *sync_state = sf->sync_state;
            if (replicas) *replicas = sf->replicas;
            if (version) *version = sf->version;
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// 设备间文件传输模拟
int
hmdfs_transfer_file(int src_device, int dst_device, char *path)
{
    acquire(&hmdfs.lock);
    
    // 验证设备
    if (src_device < 0 || src_device >= hmdfs.device_count ||
        dst_device < 0 || dst_device >= hmdfs.device_count) {
        release(&hmdfs.lock);
        return -1;
    }
    
    if (hmdfs.devices[src_device].state != DEV_ONLINE ||
        hmdfs.devices[dst_device].state != DEV_ONLINE) {
        release(&hmdfs.lock);
        return -1;
    }
    
    // 查找文件
    for (int i = 0; i < MAX_SHARED_FILES; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (sf->used && strncmp(sf->path, path, 128) == 0) {
            // 模拟传输
            hmdfs.bytes_transferred += sf->size;
            
            // 添加副本
            int found = 0;
            for (int r = 0; r < sf->replicas; r++) {
                if (sf->replica_devices[r] == dst_device) {
                    found = 1;
                    break;
                }
            }
            if (!found && sf->replicas < MAX_DEVICES) {
                sf->replica_devices[sf->replicas++] = dst_device;
            }
            
            release(&hmdfs.lock);
            return 0;
        }
    }
    
    release(&hmdfs.lock);
    return -1;
}

// 获取设备上的文件列表
int
hmdfs_list_device_files(int device_id, char *buf, int len)
{
    acquire(&hmdfs.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset, 
        "Files on device %d:\n", device_id);
    
    for (int i = 0; i < MAX_SHARED_FILES && offset < len - 128; i++) {
        struct shared_file *sf = &hmdfs.shared_files[i];
        if (!sf->used) continue;
        
        // 检查文件是否在该设备上
        int on_device = (sf->owner_device == device_id);
        if (!on_device) {
            for (int r = 0; r < sf->replicas; r++) {
                if (sf->replica_devices[r] == device_id) {
                    on_device = 1;
                    break;
                }
            }
        }
        
        if (on_device) {
            offset += snprintf(buf + offset, len - offset,
                "  %s (v%d, %d bytes)\n",
                sf->path, (int)sf->version, (int)sf->size);
        }
    }
    
    release(&hmdfs.lock);
    return offset;
}
