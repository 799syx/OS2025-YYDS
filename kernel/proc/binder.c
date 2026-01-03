// Binder IPC - Android 风格的进程间通信
// 高效的跨进程调用机制，支持对象传递和引用计数

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int snprintf(char *buf, int size, const char *fmt, ...);

// ============ Binder 常量定义 ============

#define BINDER_MAX_SERVICES 32      // 最大服务数
#define BINDER_MAX_TRANSACTIONS 64  // 最大事务数
#define BINDER_MAX_DATA 1024        // 最大数据大小
#define BINDER_SERVICE_NAME_LEN 32  // 服务名长度

// 事务类型
#define BINDER_CALL      1    // 同步调用
#define BINDER_REPLY     2    // 回复
#define BINDER_ASYNC     3    // 异步调用
#define BINDER_REGISTER  4    // 注册服务
#define BINDER_LOOKUP    5    // 查找服务

// 事务状态
#define TRANS_FREE       0
#define TRANS_PENDING    1
#define TRANS_PROCESSING 2
#define TRANS_COMPLETED  3

// ============ Binder 数据结构 ============

// Binder 服务
struct binder_service {
    int used;
    char name[BINDER_SERVICE_NAME_LEN];
    int owner_pid;              // 服务所有者进程
    int handle;                 // 服务句柄
    uint64 ptr;                 // 服务对象指针
    int ref_count;              // 引用计数
    int allow_isolated;         // 是否允许隔离进程访问
};

// Binder 事务
struct binder_transaction {
    int used;
    int id;
    int type;
    int from_pid;               // 发送方进程
    int to_pid;                 // 接收方进程
    int target_handle;          // 目标服务句柄
    int code;                   // 调用代码
    char data[BINDER_MAX_DATA]; // 数据
    int data_size;
    int status;
    uint64 reply_data;          // 回复数据地址
    int reply_size;
};

// Binder 进程状态
struct binder_proc {
    int pid;
    int ready;                  // 是否准备好接收事务
    int pending_transaction;    // 待处理事务ID
};

// ============ Binder 全局状态 ============

struct binder_state {
    struct spinlock lock;
    
    // 服务管理
    struct binder_service services[BINDER_MAX_SERVICES];
    int service_count;
    int next_handle;
    
    // 事务管理
    struct binder_transaction transactions[BINDER_MAX_TRANSACTIONS];
    int next_trans_id;
    
    // 进程状态
    struct binder_proc procs[NPROC];
    
    // 统计
    uint64 total_transactions;
    uint64 total_calls;
    uint64 total_replies;
};

static struct binder_state binder;

// ============ Binder 初始化 ============

void
binder_init(void)
{
    initlock(&binder.lock, "binder");
    
    memset(binder.services, 0, sizeof(binder.services));
    memset(binder.transactions, 0, sizeof(binder.transactions));
    memset(binder.procs, 0, sizeof(binder.procs));
    
    binder.service_count = 0;
    binder.next_handle = 1;
    binder.next_trans_id = 1;
    binder.total_transactions = 0;
    binder.total_calls = 0;
    binder.total_replies = 0;
    
    printf("binder: IPC initialized\n");
}

// ============ 服务管理 ============

// 注册服务
int
binder_register_service(char *name, uint64 ptr)
{
    acquire(&binder.lock);
    
    // 检查是否已存在
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (binder.services[i].used &&
            strncmp(binder.services[i].name, name, BINDER_SERVICE_NAME_LEN) == 0) {
            release(&binder.lock);
            return -1;  // 服务已存在
        }
    }
    
    // 找空闲槽位
    int slot = -1;
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (!binder.services[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        release(&binder.lock);
        return -1;
    }
    
    struct binder_service *svc = &binder.services[slot];
    svc->used = 1;
    strncpy(svc->name, name, BINDER_SERVICE_NAME_LEN);
    svc->owner_pid = myproc()->pid;
    svc->handle = binder.next_handle++;
    svc->ptr = ptr;
    svc->ref_count = 1;
    svc->allow_isolated = 0;
    
    binder.service_count++;
    
    release(&binder.lock);
    
    printf("binder: service '%s' registered (handle=%d)\n", name, svc->handle);
    return svc->handle;
}

// 查找服务
int
binder_lookup_service(char *name)
{
    acquire(&binder.lock);
    
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (binder.services[i].used &&
            strncmp(binder.services[i].name, name, BINDER_SERVICE_NAME_LEN) == 0) {
            int handle = binder.services[i].handle;
            binder.services[i].ref_count++;
            release(&binder.lock);
            return handle;
        }
    }
    
    release(&binder.lock);
    return -1;
}

// 释放服务引用
int
binder_release_service(int handle)
{
    acquire(&binder.lock);
    
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (binder.services[i].used && binder.services[i].handle == handle) {
            binder.services[i].ref_count--;
            if (binder.services[i].ref_count <= 0) {
                binder.services[i].used = 0;
                binder.service_count--;
            }
            release(&binder.lock);
            return 0;
        }
    }
    
    release(&binder.lock);
    return -1;
}

// ============ 事务管理 ============

// 分配事务
static struct binder_transaction*
alloc_transaction(void)
{
    for (int i = 0; i < BINDER_MAX_TRANSACTIONS; i++) {
        if (!binder.transactions[i].used) {
            binder.transactions[i].used = 1;
            binder.transactions[i].id = binder.next_trans_id++;
            return &binder.transactions[i];
        }
    }
    return 0;
}

// 发起调用
int
binder_call(int handle, int code, void *data, int size, void *reply, int reply_size)
{
    if (size > BINDER_MAX_DATA)
        return -1;
    
    acquire(&binder.lock);
    
    // 查找目标服务
    struct binder_service *target = 0;
    for (int i = 0; i < BINDER_MAX_SERVICES; i++) {
        if (binder.services[i].used && binder.services[i].handle == handle) {
            target = &binder.services[i];
            break;
        }
    }
    
    if (target == 0) {
        release(&binder.lock);
        return -1;
    }
    
    // 分配事务
    struct binder_transaction *trans = alloc_transaction();
    if (trans == 0) {
        release(&binder.lock);
        return -1;
    }
    
    trans->type = BINDER_CALL;
    trans->from_pid = myproc()->pid;
    trans->to_pid = target->owner_pid;
    trans->target_handle = handle;
    trans->code = code;
    trans->data_size = size;
    trans->reply_data = (uint64)reply;
    trans->reply_size = reply_size;
    trans->status = TRANS_PENDING;
    
    if (data && size > 0) {
        memmove(trans->data, data, size);
    }
    
    binder.total_transactions++;
    binder.total_calls++;
    
    // 通知目标进程
    for (int i = 0; i < NPROC; i++) {
        if (binder.procs[i].pid == target->owner_pid) {
            binder.procs[i].pending_transaction = trans->id;
            break;
        }
    }
    
    release(&binder.lock);
    
    // 等待回复（简化实现）
    // 实际应该阻塞等待
    
    return trans->id;
}

// 回复调用
int
binder_reply(int trans_id, void *data, int size)
{
    if (size > BINDER_MAX_DATA)
        return -1;
    
    acquire(&binder.lock);
    
    struct binder_transaction *trans = 0;
    for (int i = 0; i < BINDER_MAX_TRANSACTIONS; i++) {
        if (binder.transactions[i].used && binder.transactions[i].id == trans_id) {
            trans = &binder.transactions[i];
            break;
        }
    }
    
    if (trans == 0 || trans->status != TRANS_PROCESSING) {
        release(&binder.lock);
        return -1;
    }
    
    // 复制回复数据
    if (data && size > 0 && trans->reply_data) {
        // 实际需要通过页表复制到调用方
        memmove(trans->data, data, size);
        trans->data_size = size;
    }
    
    trans->status = TRANS_COMPLETED;
    binder.total_replies++;
    
    release(&binder.lock);
    return 0;
}

// 接收事务
int
binder_receive(int *trans_id, int *code, void *data, int *size)
{
    acquire(&binder.lock);
    
    int pid = myproc()->pid;
    
    // 查找待处理事务
    for (int i = 0; i < BINDER_MAX_TRANSACTIONS; i++) {
        struct binder_transaction *trans = &binder.transactions[i];
        if (trans->used && trans->to_pid == pid && trans->status == TRANS_PENDING) {
            trans->status = TRANS_PROCESSING;
            
            *trans_id = trans->id;
            *code = trans->code;
            if (data && trans->data_size > 0) {
                memmove(data, trans->data, trans->data_size);
            }
            *size = trans->data_size;
            
            release(&binder.lock);
            return 0;
        }
    }
    
    release(&binder.lock);
    return -1;  // 没有待处理事务
}

// ============ 列表和统计 ============

int
binder_list_services(char *buf, int len)
{
    acquire(&binder.lock);
    
    int offset = 0;
    offset += snprintf(buf + offset, len - offset,
        "HANDLE\tNAME\t\t\tOWNER\tREFS\n");
    
    for (int i = 0; i < BINDER_MAX_SERVICES && offset < len - 64; i++) {
        struct binder_service *svc = &binder.services[i];
        if (svc->used) {
            offset += snprintf(buf + offset, len - offset,
                "%d\t%s\t\t%d\t%d\n",
                svc->handle, svc->name, svc->owner_pid, svc->ref_count);
        }
    }
    
    release(&binder.lock);
    return offset;
}

void
binder_print_stats(void)
{
    acquire(&binder.lock);
    
    printf("\n=== Binder IPC Statistics ===\n");
    printf("Services: %d\n", binder.service_count);
    printf("Total transactions: %d\n", (int)binder.total_transactions);
    printf("Total calls: %d\n", (int)binder.total_calls);
    printf("Total replies: %d\n", (int)binder.total_replies);
    printf("=============================\n");
    
    release(&binder.lock);
}
