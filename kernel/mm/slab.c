// Slab 分配器实现
// 用于高效管理小对象内存分配（32, 64, 128, 256, 512 字节）

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// Slab 大小类别
#define SLAB_SIZES 5
static int slab_sizes[SLAB_SIZES] = {32, 64, 128, 256, 512};

// 每个 slab 对象的头部（用于空闲链表）
struct slab_obj {
    struct slab_obj *next;
};

// Slab 页面头部信息
struct slab_page {
    struct slab_page *next;     // 下一个 slab 页面
    int obj_size;               // 对象大小
    int total_objs;             // 总对象数
    int free_objs;              // 空闲对象数
    struct slab_obj *freelist;  // 空闲对象链表
    char *start;                // 对象区域起始地址
};

// Slab 缓存（每种大小一个）
struct slab_cache {
    struct spinlock lock;
    int obj_size;               // 对象大小
    struct slab_page *partial;  // 部分使用的页面
    struct slab_page *full;     // 完全使用的页面
    struct slab_page *empty;    // 完全空闲的页面
    
    // 统计信息
    uint64 alloc_count;         // 分配次数
    uint64 free_count;          // 释放次数
    int total_pages;            // 总页面数
};

static struct slab_cache caches[SLAB_SIZES];

// 全局 slab 统计
struct slab_stats {
    uint64 total_allocs;
    uint64 total_frees;
    uint64 cache_hits;
    uint64 cache_misses;
};
static struct slab_stats slab_stats;
static struct spinlock slab_stats_lock;

// 初始化单个 slab 页面
static struct slab_page*
slab_page_init(int obj_size)
{
    char *page = kalloc();
    if (page == 0)
        return 0;
    
    memset(page, 0, PGSIZE);
    
    // 页面头部放在页面开始处
    struct slab_page *sp = (struct slab_page *)page;
    sp->obj_size = obj_size;
    sp->next = 0;
    
    // 计算可以放多少个对象（跳过头部）
    int header_size = sizeof(struct slab_page);
    header_size = (header_size + 15) & ~15;  // 16字节对齐
    
    sp->start = page + header_size;
    sp->total_objs = (PGSIZE - header_size) / obj_size;
    sp->free_objs = sp->total_objs;
    
    // 构建空闲链表
    sp->freelist = 0;
    for (int i = sp->total_objs - 1; i >= 0; i--) {
        struct slab_obj *obj = (struct slab_obj *)(sp->start + i * obj_size);
        obj->next = sp->freelist;
        sp->freelist = obj;
    }
    
    return sp;
}

// 初始化 slab 分配器
void
slab_init(void)
{
    initlock(&slab_stats_lock, "slab_stats");
    memset(&slab_stats, 0, sizeof(slab_stats));
    
    for (int i = 0; i < SLAB_SIZES; i++) {
        char name[16];
        snprintf(name, sizeof(name), "slab_%d", slab_sizes[i]);
        initlock(&caches[i].lock, name);
        caches[i].obj_size = slab_sizes[i];
        caches[i].partial = 0;
        caches[i].full = 0;
        caches[i].empty = 0;
        caches[i].alloc_count = 0;
        caches[i].free_count = 0;
        caches[i].total_pages = 0;
    }
    
    printf("slab: initialized with %d size classes\n", SLAB_SIZES);
}

// 找到合适的缓存索引
static int
find_cache_index(int size)
{
    for (int i = 0; i < SLAB_SIZES; i++) {
        if (size <= slab_sizes[i])
            return i;
    }
    return -1;  // 太大，使用 kalloc
}

// 从 slab 分配内存
void*
slab_alloc(int size)
{
    int idx = find_cache_index(size);
    if (idx < 0) {
        // 对象太大，回退到页面分配
        return kalloc();
    }
    
    struct slab_cache *cache = &caches[idx];
    acquire(&cache->lock);
    
    struct slab_page *sp = cache->partial;
    
    // 如果没有部分使用的页面，尝试从空闲页面获取
    if (sp == 0) {
        sp = cache->empty;
        if (sp) {
            cache->empty = sp->next;
            sp->next = cache->partial;
            cache->partial = sp;
        }
    }
    
    // 如果还是没有，分配新页面
    if (sp == 0) {
        sp = slab_page_init(cache->obj_size);
        if (sp == 0) {
            release(&cache->lock);
            return 0;
        }
        sp->next = cache->partial;
        cache->partial = sp;
        cache->total_pages++;
        
        acquire(&slab_stats_lock);
        slab_stats.cache_misses++;
        release(&slab_stats_lock);
    } else {
        acquire(&slab_stats_lock);
        slab_stats.cache_hits++;
        release(&slab_stats_lock);
    }
    
    // 从空闲链表分配
    struct slab_obj *obj = sp->freelist;
    sp->freelist = obj->next;
    sp->free_objs--;
    
    // 如果页面满了，移到 full 链表
    if (sp->free_objs == 0) {
        cache->partial = sp->next;
        sp->next = cache->full;
        cache->full = sp;
    }
    
    cache->alloc_count++;
    release(&cache->lock);
    
    acquire(&slab_stats_lock);
    slab_stats.total_allocs++;
    release(&slab_stats_lock);
    
    memset(obj, 0, cache->obj_size);
    return (void *)obj;
}

// 释放 slab 内存
void
slab_free(void *ptr, int size)
{
    if (ptr == 0)
        return;
    
    int idx = find_cache_index(size);
    if (idx < 0) {
        // 对象太大，使用 kfree
        kfree(ptr);
        return;
    }
    
    struct slab_cache *cache = &caches[idx];
    acquire(&cache->lock);
    
    // 找到对象所在的页面
    uint64 page_addr = PGROUNDDOWN((uint64)ptr);
    struct slab_page *sp = 0;
    struct slab_page **prev = 0;
    
    // 先在 full 链表中查找
    prev = &cache->full;
    for (sp = cache->full; sp; sp = sp->next) {
        if ((uint64)sp == page_addr)
            break;
        prev = &sp->next;
    }
    
    if (sp) {
        // 从 full 移到 partial
        *prev = sp->next;
        sp->next = cache->partial;
        cache->partial = sp;
    } else {
        // 在 partial 链表中查找
        for (sp = cache->partial; sp; sp = sp->next) {
            if ((uint64)sp == page_addr)
                break;
        }
    }
    
    if (sp == 0) {
        release(&cache->lock);
        printf("slab_free: invalid pointer %p\n", ptr);
        return;
    }
    
    // 归还到空闲链表
    struct slab_obj *obj = (struct slab_obj *)ptr;
    obj->next = sp->freelist;
    sp->freelist = obj;
    sp->free_objs++;
    
    // 如果页面完全空闲，移到 empty 链表
    if (sp->free_objs == sp->total_objs) {
        // 从 partial 移除
        prev = &cache->partial;
        for (struct slab_page *p = cache->partial; p; p = p->next) {
            if (p == sp) {
                *prev = sp->next;
                break;
            }
            prev = &p->next;
        }
        sp->next = cache->empty;
        cache->empty = sp;
    }
    
    cache->free_count++;
    release(&cache->lock);
    
    acquire(&slab_stats_lock);
    slab_stats.total_frees++;
    release(&slab_stats_lock);
}

// 获取 slab 统计信息
void
slab_get_stats(uint64 *total_allocs, uint64 *total_frees, 
               uint64 *cache_hits, uint64 *cache_misses)
{
    acquire(&slab_stats_lock);
    if (total_allocs) *total_allocs = slab_stats.total_allocs;
    if (total_frees) *total_frees = slab_stats.total_frees;
    if (cache_hits) *cache_hits = slab_stats.cache_hits;
    if (cache_misses) *cache_misses = slab_stats.cache_misses;
    release(&slab_stats_lock);
}

// 打印 slab 统计信息
void
slab_print_stats(void)
{
    printf("\n=== Slab Allocator Statistics ===\n");
    printf("Total allocations: %d\n", slab_stats.total_allocs);
    printf("Total frees: %d\n", slab_stats.total_frees);
    printf("Cache hits: %d\n", slab_stats.cache_hits);
    printf("Cache misses: %d\n", slab_stats.cache_misses);
    printf("Hit rate: %d%%\n", 
           slab_stats.total_allocs > 0 ? 
           (int)(slab_stats.cache_hits * 100 / slab_stats.total_allocs) : 0);
    
    printf("\nPer-cache statistics:\n");
    for (int i = 0; i < SLAB_SIZES; i++) {
        struct slab_cache *c = &caches[i];
        printf("  [%3d bytes] pages=%d allocs=%d frees=%d\n",
               c->obj_size, c->total_pages, 
               (int)c->alloc_count, (int)c->free_count);
    }
    printf("=================================\n");
}

// 收缩 slab 缓存（释放空闲页面）
int
slab_shrink(void)
{
    int freed = 0;
    
    for (int i = 0; i < SLAB_SIZES; i++) {
        struct slab_cache *cache = &caches[i];
        acquire(&cache->lock);
        
        // 释放所有空闲页面
        while (cache->empty) {
            struct slab_page *sp = cache->empty;
            cache->empty = sp->next;
            kfree((void *)sp);
            cache->total_pages--;
            freed++;
        }
        
        release(&cache->lock);
    }
    
    return freed;
}
