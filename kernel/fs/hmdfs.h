// HMDFS - 分布式文件系统头文件

#ifndef _HMDFS_H_
#define _HMDFS_H_

#include "types.h"

// 设备类型
#define DEV_TYPE_LOCAL   0
#define DEV_TYPE_PHONE   1
#define DEV_TYPE_TABLET  2
#define DEV_TYPE_PC      3
#define DEV_TYPE_WATCH   4
#define DEV_TYPE_TV      5

// 冲突解决策略
#define POLICY_NEWEST_WINS  0
#define POLICY_LARGEST_WINS 1
#define POLICY_MANUAL       2
#define POLICY_MERGE        3

// 初始化
void hmdfs_init(void);

// 设备管理
int hmdfs_register_device(char *name, char *uuid, int type);
int hmdfs_device_offline(int device_id);
int hmdfs_list_devices(char *buf, int len);

// 文件共享
int hmdfs_share_file(char *path);
int hmdfs_unshare_file(char *path);
int hmdfs_list_shared(char *buf, int len);

// 同步管理
int hmdfs_sync(void);
int hmdfs_mark_dirty(char *path);

// 冲突处理
int hmdfs_check_conflict(char *path, int device_id);
int hmdfs_resolve_conflict(char *path, int winner_device);

// 缓存管理
int hmdfs_cache_add(char *path, int device_id);
int hmdfs_cache_invalidate(char *path);
int hmdfs_cache_flush(void);

// 统计信息
void hmdfs_print_stats(void);
void hmdfs_get_stats(uint64 *files_synced, uint64 *bytes_transferred,
                     uint64 *conflicts_resolved, uint64 *cache_hits);

// 配置
void hmdfs_set_config(int auto_sync, int sync_interval, int max_file_size);
void hmdfs_set_conflict_policy(int policy);

#endif // _HMDFS_H_
