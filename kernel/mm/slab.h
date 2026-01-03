// Slab 分配器头文件

#ifndef _SLAB_H_
#define _SLAB_H_

// 初始化 slab 分配器
void slab_init(void);

// 从 slab 分配内存
void* slab_alloc(int size);

// 释放 slab 内存
void slab_free(void *ptr, int size);

// 获取 slab 统计信息
void slab_get_stats(uint64 *total_allocs, uint64 *total_frees, 
                    uint64 *cache_hits, uint64 *cache_misses);

// 打印 slab 统计信息
void slab_print_stats(void);

// 收缩 slab 缓存（释放空闲页面）
int slab_shrink(void);

#endif // _SLAB_H_
