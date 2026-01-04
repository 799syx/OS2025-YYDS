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

// ============ Ability 间通信 (IPC) ============

#define MAX_ABILITY_MESSAGES 64
#define MAX_MESSAGE_DATA 256

// 消息类型
#define MSG_REQUEST  1    // 请求
#define MSG_RESPONSE 2    // 响应
#define MSG_EVENT    3    // 事件通知

// Ability 消息
struct ability_message {
    int used;
    int id;
    int type;                       // 消息类型
    int src_ability;                // 源 Ability ID
    int dst_ability;                // 目标 Ability ID
    int request_id;                 // 请求ID (用于匹配响应)
    char data[MAX_MESSAGE_DATA];    // 消息数据
    int data_len;
    uint64 timestamp;
    int processed;                  // 是否已处理
};

// 消息队列
static struct {
    struct spinlock lock;
    struct ability_message messages[MAX_ABILITY_MESSAGES];
    int next_msg_id;
    int next_request_id;
    uint64 total_messages;
    uint64 total_requests;
    uint64 total_responses;
} ability_mq;

// 初始化消息队列
void
ability_mq_init(void)
{
    initlock(&ability_mq.lock, "ability_mq");
    memset(ability_mq.messages, 0, sizeof(ability_mq.messages));
    ability_mq.next_msg_id = 1;
    ability_mq.next_request_id = 1;
    ability_mq.total_messages = 0;
    ability_mq.total_requests = 0;
    ability_mq.total_responses = 0;
}

// 发送消息到另一个 Ability
int
ability_send_message(int src_id, int dst_id, int type, char *data, int len)
{
    if (len > MAX_MESSAGE_DATA) return -1;
    
    acquire(&ability_mq.lock);
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
        if (!ability_mq.messages[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&ability_mq.lock);
        return -1;
    }
    
    struct ability_message *msg = &ability_mq.messages[slot];
    msg->used = 1;
    msg->id = ability_mq.next_msg_id++;
    msg->type = type;
    msg->src_ability = src_id;
    msg->dst_ability = dst_id;
    msg->request_id = (type == MSG_REQUEST) ? ability_mq.next_request_id++ : 0;
    if (data && len > 0) {
        memmove(msg->data, data, len);
        msg->data_len = len;
    } else {
        msg->data_len = 0;
    }
    extern uint ticks;
    msg->timestamp = ticks;
    msg->processed = 0;
    
    ability_mq.total_messages++;
    if (type == MSG_REQUEST) ability_mq.total_requests++;
    if (type == MSG_RESPONSE) ability_mq.total_responses++;
    
    release(&ability_mq.lock);
    return msg->id;
}

// 接收消息
int
ability_recv_message(int ability_id, char *buf, int buflen, int *src_id, int *type)
{
    acquire(&ability_mq.lock);
    
    for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
        struct ability_message *msg = &ability_mq.messages[i];
        if (msg->used && msg->dst_ability == ability_id && !msg->processed) {
            // 复制消息数据
            int copy_len = msg->data_len < buflen ? msg->data_len : buflen;
            if (buf && copy_len > 0) {
                memmove(buf, msg->data, copy_len);
            }
            if (src_id) *src_id = msg->src_ability;
            if (type) *type = msg->type;
            
            msg->processed = 1;
            msg->used = 0;  // 释放消息槽位
            
            release(&ability_mq.lock);
            return copy_len;
        }
    }
    
    release(&ability_mq.lock);
    return -1;  // 无消息
}

// 发送请求并等待响应 (同步调用)
int
ability_call(int src_id, int dst_id, char *request, int req_len, 
             char *response, int resp_len)
{
    // 发送请求
    int msg_id = ability_send_message(src_id, dst_id, MSG_REQUEST, request, req_len);
    if (msg_id < 0) return -1;
    
    // 获取请求ID
    int request_id = 0;
    acquire(&ability_mq.lock);
    for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
        if (ability_mq.messages[i].id == msg_id) {
            request_id = ability_mq.messages[i].request_id;
            break;
        }
    }
    release(&ability_mq.lock);
    
    // 等待响应 (简化实现，实际应该使用睡眠/唤醒机制)
    extern uint ticks;
    uint64 timeout = ticks + 1000;  // 10秒超时
    
    while (ticks < timeout) {
        acquire(&ability_mq.lock);
        for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
            struct ability_message *msg = &ability_mq.messages[i];
            if (msg->used && msg->type == MSG_RESPONSE && 
                msg->dst_ability == src_id && msg->request_id == request_id) {
                // 找到响应
                int copy_len = msg->data_len < resp_len ? msg->data_len : resp_len;
                if (response && copy_len > 0) {
                    memmove(response, msg->data, copy_len);
                }
                msg->used = 0;
                release(&ability_mq.lock);
                return copy_len;
            }
        }
        release(&ability_mq.lock);
        
        // 让出CPU
        yield();
    }
    
    return -1;  // 超时
}

// 发送响应
int
ability_reply(int src_id, int dst_id, int request_id, char *data, int len)
{
    if (len > MAX_MESSAGE_DATA) return -1;
    
    acquire(&ability_mq.lock);
    
    int slot = -1;
    for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
        if (!ability_mq.messages[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&ability_mq.lock);
        return -1;
    }
    
    struct ability_message *msg = &ability_mq.messages[slot];
    msg->used = 1;
    msg->id = ability_mq.next_msg_id++;
    msg->type = MSG_RESPONSE;
    msg->src_ability = src_id;
    msg->dst_ability = dst_id;
    msg->request_id = request_id;
    if (data && len > 0) {
        memmove(msg->data, data, len);
        msg->data_len = len;
    }
    extern uint ticks;
    msg->timestamp = ticks;
    msg->processed = 0;
    
    ability_mq.total_responses++;
    
    release(&ability_mq.lock);
    return msg->id;
}

// ============ 生命周期管理增强 ============

// 生命周期回调类型
typedef void (*lifecycle_callback)(int ability_id, int event);

// 生命周期监听器
struct lifecycle_listener {
    int used;
    int ability_id;         // 监听的 Ability (-1 表示所有)
    lifecycle_callback cb;  // 回调函数
};

#define MAX_LIFECYCLE_LISTENERS 16
static struct lifecycle_listener lifecycle_listeners[MAX_LIFECYCLE_LISTENERS];

// 注册生命周期监听器
int
ability_add_lifecycle_listener(int ability_id, lifecycle_callback cb)
{
    acquire(&ability_mgr.lock);
    
    for (int i = 0; i < MAX_LIFECYCLE_LISTENERS; i++) {
        if (!lifecycle_listeners[i].used) {
            lifecycle_listeners[i].used = 1;
            lifecycle_listeners[i].ability_id = ability_id;
            lifecycle_listeners[i].cb = cb;
            release(&ability_mgr.lock);
            return i;
        }
    }
    
    release(&ability_mgr.lock);
    return -1;
}

// 移除生命周期监听器
int
ability_remove_lifecycle_listener(int listener_id)
{
    if (listener_id < 0 || listener_id >= MAX_LIFECYCLE_LISTENERS)
        return -1;
    
    acquire(&ability_mgr.lock);
    lifecycle_listeners[listener_id].used = 0;
    release(&ability_mgr.lock);
    return 0;
}

// 触发生命周期事件
static void
trigger_lifecycle_event(int ability_id, int event)
{
    for (int i = 0; i < MAX_LIFECYCLE_LISTENERS; i++) {
        if (lifecycle_listeners[i].used) {
            if (lifecycle_listeners[i].ability_id == -1 || 
                lifecycle_listeners[i].ability_id == ability_id) {
                if (lifecycle_listeners[i].cb) {
                    lifecycle_listeners[i].cb(ability_id, event);
                }
            }
        }
    }
}

// 增强的生命周期状态转换
int
ability_set_state(int ability_id, int new_state)
{
    acquire(&ability_mgr.lock);
    
    for (int i = 0; i < MAX_ABILITIES; i++) {
        struct ability_info *ab = &ability_mgr.abilities[i];
        if (ab->used && ab->id == ability_id) {
            int old_state = ab->state;
            ab->state = new_state;
            
            // 触发相应的生命周期事件
            int event = 0;
            if (old_state == ABILITY_INITIAL && new_state == ABILITY_INACTIVE)
                event = LIFECYCLE_CREATE;
            else if (new_state == ABILITY_ACTIVE)
                event = LIFECYCLE_FOREGROUND;
            else if (new_state == ABILITY_BACKGROUND)
                event = LIFECYCLE_BACKGROUND;
            else if (new_state == ABILITY_INACTIVE)
                event = LIFECYCLE_STOP;
            
            if (event) {
                ability_mgr.lifecycle_events++;
                release(&ability_mgr.lock);
                trigger_lifecycle_event(ability_id, event);
                return 0;
            }
            
            release(&ability_mgr.lock);
            return 0;
        }
    }
    
    release(&ability_mgr.lock);
    return -1;
}

// 获取 Ability 状态
int
ability_get_state(int ability_id)
{
    acquire(&ability_mgr.lock);
    
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used && 
            ability_mgr.abilities[i].id == ability_id) {
            int state = ability_mgr.abilities[i].state;
            release(&ability_mgr.lock);
            return state;
        }
    }
    
    release(&ability_mgr.lock);
    return -1;
}

// 连接两个 Ability (建立通信通道)
int
ability_connect(int src_id, int dst_id)
{
    // 验证两个 Ability 都存在
    acquire(&ability_mgr.lock);
    
    int src_found = 0, dst_found = 0;
    for (int i = 0; i < MAX_ABILITIES; i++) {
        if (ability_mgr.abilities[i].used) {
            if (ability_mgr.abilities[i].id == src_id) src_found = 1;
            if (ability_mgr.abilities[i].id == dst_id) dst_found = 1;
        }
    }
    
    release(&ability_mgr.lock);
    
    if (!src_found || !dst_found) return -1;
    
    // 连接成功 (实际上消息队列已经支持任意 Ability 间通信)
    return 0;
}

// 断开 Ability 连接
int
ability_disconnect(int src_id, int dst_id)
{
    // 清除两个 Ability 之间的所有待处理消息
    acquire(&ability_mq.lock);
    
    for (int i = 0; i < MAX_ABILITY_MESSAGES; i++) {
        struct ability_message *msg = &ability_mq.messages[i];
        if (msg->used && 
            ((msg->src_ability == src_id && msg->dst_ability == dst_id) ||
             (msg->src_ability == dst_id && msg->dst_ability == src_id))) {
            msg->used = 0;
        }
    }
    
    release(&ability_mq.lock);
    return 0;
}

// 获取 Ability IPC 统计
void
ability_get_ipc_stats(uint64 *total_msgs, uint64 *total_reqs, uint64 *total_resps)
{
    acquire(&ability_mq.lock);
    if (total_msgs) *total_msgs = ability_mq.total_messages;
    if (total_reqs) *total_reqs = ability_mq.total_requests;
    if (total_resps) *total_resps = ability_mq.total_responses;
    release(&ability_mq.lock);
}
