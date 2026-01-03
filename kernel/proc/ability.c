// Ability - 鸿蒙风格的应用能力框架
// 支持 FA (Feature Ability) 和 PA (Particle Ability)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// ============ Ability 常量 ============

#define MAX_ABILITIES 32
#define MAX_ABILITY_NAME 32
#define MAX_BUNDLE_NAME 64
#define MAX_WANT_DATA 256

// Ability 类型
#define ABILITY_PAGE     1    // 页面能力 (FA)
#define ABILITY_SERVICE  2    // 服务能力 (PA)
#define ABILITY_DATA     3    // 数据能力 (PA)

// Ability 状态
#define ABILITY_INITIAL     0
#define ABILITY_INACTIVE    1
#define ABILITY_ACTIVE      2
#define ABILITY_BACKGROUND  3

// 生命周期事件
#define LIFECYCLE_CREATE    1
#define LIFECYCLE_START     2
#define LIFECYCLE_STOP      3
#define LIFECYCLE_DESTROY   4
#define LIFECYCLE_FOREGROUND 5
#define LIFECYCLE_BACKGROUND 6

// ============ Ability 数据结构 ============

// Want - 意图，用于启动 Ability
struct want {
    char bundle_name[MAX_BUNDLE_NAME];  // 包名
    char ability_name[MAX_ABILITY_NAME]; // Ability 名
    int flags;
    char data[MAX_WANT_DATA];           // 附加数据
    int data_len;
};

// Ability 信息
struct ability_info {
    int used;
    int id;
    char name[MAX_ABILITY_NAME];
    char bundle[MAX_BUNDLE_NAME];
    int type;                   // PAGE/SERVICE/DATA
    int state;
    int owner_pid;              // 所属进程
    uint64 created_time;
    uint64 start_time;
    int visible;                // 是否可见
    int permission;             // 权限级别
};

// Ability 栈 (用于页面导航)
struct ability_stack {
    int abilities[16];          // Ability ID 栈
    int top;
};

// ============ Ability 全局状态 ============

struct ability_manager {
    struct spinlock lock;
    
    struct ability_info abilities[MAX_ABILITIES];
    int ability_count;
    int next_id;
    
    // 前台 Ability 栈
    struct ability_stack fg_stack;
    
    // 统计
    uint64 total_starts;
    uint64 total_stops;
    uint64 lifecycle_events;
};

static struct ability_manager ability_mgr;

// ============ 初始化 ============

void
ability_init(void)
{
    initlock(&ability_mgr.lock, "ability");
    
    memset(ability_mgr.abilities, 0, sizeof(ability_mgr.abilities));
    ability_mgr.ability_count = 0;
    ability_mgr.next_id = 1;
    ability_mgr.fg_stack.top = 0;
    ability_mgr.total_starts = 0;
    ability_mgr.total_stops = 0;
    ability_mgr.lifecycle_events = 0;
    
    printf("ability: framework initialized\n");
}

// ============ Ability 生命周期 ============

// 注册 Ability
int
ability_register(char *bundle, char *name, int type)
{
    acquire(&ability_mgr.lock);
    
    // 检查是否已存在
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used &&
            strncmp(ability_mgr.abilities[i].bundle, bundle, MAX_BUNDLE_NAME) == 0 &&
            strncmp(ability_mgr.abilities[i].name, name, MAX_ABILITY_NAME) == 0) {
            release(&ability_mgr.lock);
            return ability_mgr.abilities[i].id;  // 已存在
        }
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (!ability_mgr.abilities[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&ability_mgr.lock);
        return -1;
    }
    
    struct ability_info *ab = &ability_mgr.abilities[slot];
    ab->used = 1;
    ab->id = ability_mgr.next_id++;
    strncpy(ab->name, name, MAX_ABILITY_NAME);
    strncpy(ab->bundle, bundle, MAX_BUNDLE_NAME);
    ab->type = type;
    ab->state = ABILITY_INITIAL;
    ab->owner_pid = myproc()->pid;
    extern uint ticks;
    ab->created_time = ticks;
    ab->visible = 0;
    ab->permission = 0;
    
    ability_mgr.ability_count++;
    ability_mgr.lifecycle_events++;
    
    release(&ability_mgr.lock);
    
    printf("ability: '%s/%s' registered (id=%d, type=%d)\n", 
           bundle, name, ab->id, type);
    return ab->id;
}

// 启动 Ability
int
ability_start(int ability_id)
{
    acquire(&ability_mgr.lock);
    
    struct ability_info *ab = 0;
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used && 
            ability_mgr.abilities[i].id == ability_id) {
            ab = &ability_mgr.abilities[i];
            break;
        }
    }
    
    if (ab == 0) {
        release(&ability_mgr.lock);
        return -1;
    }
    
    if (ab->state == ABILITY_ACTIVE) {
        release(&ability_mgr.lock);
        return 0;  // 已经是活动状态
    }
    
    ab->state = ABILITY_ACTIVE;
    extern uint ticks;
    ab->start_time = ticks;
    ab->visible = (ab->type == ABILITY_PAGE);
    
    // 如果是页面 Ability，加入前台栈
    if (ab->type == ABILITY_PAGE && ability_mgr.fg_stack.top < 16) {
        ability_mgr.fg_stack.abilities[ability_mgr.fg_stack.top++] = ability_id;
    }
    
    ability_mgr.total_starts++;
    ability_mgr.lifecycle_events++;
    
    release(&ability_mgr.lock);
    return 0;
}

// 停止 Ability
int
ability_stop(int ability_id)
{
    acquire(&ability_mgr.lock);
    
    struct ability_info *ab = 0;
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used && 
            ability_mgr.abilities[i].id == ability_id) {
            ab = &ability_mgr.abilities[i];
            break;
        }
    }
    
    if (ab == 0) {
        release(&ability_mgr.lock);
        return -1;
    }
    
    ab->state = ABILITY_INACTIVE;
    ab->visible = 0;
    
    // 从前台栈移除
    for (int i = 0; i < ability_mgr.fg_stack.top; i++) {
        if (ability_mgr.fg_stack.abilities[i] == ability_id) {
            for (int j = i; j < ability_mgr.fg_stack.top - 1; j++) {
                ability_mgr.fg_stack.abilities[j] = ability_mgr.fg_stack.abilities[j + 1];
            }
            ability_mgr.fg_stack.top--;
            break;
        }
    }
    
    ability_mgr.total_stops++;
    ability_mgr.lifecycle_events++;
    
    release(&ability_mgr.lock);
    return 0;
}

// 销毁 Ability
int
ability_destroy(int ability_id)
{
    acquire(&ability_mgr.lock);
    
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used && 
            ability_mgr.abilities[i].id == ability_id) {
            ability_mgr.abilities[i].used = 0;
            ability_mgr.ability_count--;
            ability_mgr.lifecycle_events++;
            
            // 从前台栈移除
            for (int j = 0; j < ability_mgr.fg_stack.top; j++) {
                if (ability_mgr.fg_stack.abilities[j] == ability_id) {
                    for (int k = j; k < ability_mgr.fg_stack.top - 1; k++) {
                        ability_mgr.fg_stack.abilities[k] = ability_mgr.fg_stack.abilities[k + 1];
                    }
                    ability_mgr.fg_stack.top--;
                    break;
                }
            }
            
            release(&ability_mgr.lock);
            return 0;
        }
    }
    
    release(&ability_mgr.lock);
    return -1;
}

// ============ Want 处理 ============

// 通过 Want 启动 Ability
int
ability_start_want(struct want *w)
{
    acquire(&ability_mgr.lock);
    
    // 查找匹配的 Ability
    for (int i = 0; i < MAX_ABILITIES; i++) {
        struct ability_info *ab = &ability_mgr.abilities[i];
        if (ab->used &&
            strncmp(ab->bundle, w->bundle_name, MAX_BUNDLE_NAME) == 0 &&
            strncmp(ab->name, w->ability_name, MAX_ABILITY_NAME) == 0) {
            
            int id = ab->id;
            release(&ability_mgr.lock);
            return ability_start(id);
        }
    }
    
    release(&ability_mgr.lock);
    return -1;  // 未找到
}

// ============ 页面导航 ============

// 返回上一个页面
int
ability_back(void)
{
    acquire(&ability_mgr.lock);
    
    if (ability_mgr.fg_stack.top <= 1) {
        release(&ability_mgr.lock);
        return -1;  // 没有可返回的页面
    }
    
    // 停止当前页面
    int current_id = ability_mgr.fg_stack.abilities[ability_mgr.fg_stack.top - 1];
    ability_mgr.fg_stack.top--;
    
    // 找到当前 Ability 并设为后台
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used && 
            ability_mgr.abilities[i].id == current_id) {
            ability_mgr.abilities[i].state = ABILITY_BACKGROUND;
            ability_mgr.abilities[i].visible = 0;
            break;
        }
    }
    
    // 恢复上一个页面
    if (ability_mgr.fg_stack.top > 0) {
        int prev_id = ability_mgr.fg_stack.abilities[ability_mgr.fg_stack.top - 1];
        for (int i = 0; i < MAX_ABILITIES; i++) {
            if (ability_mgr.abilities[i].used && 
                ability_mgr.abilities[i].id == prev_id) {
                ability_mgr.abilities[i].state = ABILITY_ACTIVE;
                ability_mgr.abilities[i].visible = 1;
                break;
            }
        }
    }
    
    ability_mgr.lifecycle_events++;
    
    release(&ability_mgr.lock);
    return 0;
}

// 获取当前前台 Ability
int
ability_get_foreground(void)
{
    acquire(&ability_mgr.lock);
    
    int id = -1;
    if (ability_mgr.fg_stack.top > 0) {
        id = ability_mgr.fg_stack.abilities[ability_mgr.fg_stack.top - 1];
    }
    
    release(&ability_mgr.lock);
    return id;
}

// ============ 列表和统计 ============

int
ability_list(char *buf, int len)
{
    acquire(&ability_mgr.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "ID\tBUNDLE\t\tNAME\t\tTYPE\tSTATE\n");
    
    char *type_names[] = {"", "PAGE", "SERVICE", "DATA"};
    char *state_names[] = {"INITIAL", "INACTIVE", "ACTIVE", "BACKGROUND"};
    
    for (int i = 0; i < MAX_ABILITIES && offset < len - 80; i++) {
        struct ability_info *ab = &ability_mgr.abilities[i];
        if (ab->used) {
            offset += snprintf(buf + offset, len - offset,
                "%d\t%s\t%s\t\t%s\t%s\n",
                ab->id, ab->bundle, ab->name,
                type_names[ab->type], state_names[ab->state]);
        }
    }
    
    release(&ability_mgr.lock);
    return offset;
}

void
ability_print_stats(void)
{
    acquire(&ability_mgr.lock);
    
    printf("\n=== Ability Framework Statistics ===\n");
    printf("Total abilities: %d\n", ability_mgr.ability_count);
    printf("Foreground stack depth: %d\n", ability_mgr.fg_stack.top);
    printf("Total starts: %d\n", (int)ability_mgr.total_starts);
    printf("Total stops: %d\n", (int)ability_mgr.total_stops);
    printf("Lifecycle events: %d\n", (int)ability_mgr.lifecycle_events);
    printf("====================================\n");
    
    release(&ability_mgr.lock);
}
