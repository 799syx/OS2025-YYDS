// QoS (Quality of Service) 调度器头文件

#ifndef _QOS_H_
#define _QOS_H_

#include "types.h"

// QoS 级别定义
#define QOS_USER_INTERACTIVE  0   // 用户交互（最高优先级）
#define QOS_DEADLINE_REQUEST  1   // 截止时间请求
#define QOS_USER_INITIATED    2   // 用户发起
#define QOS_DEFAULT           3   // 默认级别
#define QOS_UTILITY           4   // 实用工具
#define QOS_BACKGROUND        5   // 后台任务（最低优先级）
#define QOS_LEVELS            6

// 初始化 QoS 调度器
void qos_init(void);

// 注册进程到 QoS 调度器
int qos_register(struct proc *p, int qos_level);

// 注销进程
void qos_unregister(struct proc *p);

// 设置进程的 QoS 级别
int qos_set_level(struct proc *p, int qos_level);

// 获取进程的 QoS 级别
int qos_get_level(struct proc *p);

// 设置截止时间
int qos_set_deadline(struct proc *p, uint64 deadline);

// QoS 调度决策
struct proc* qos_schedule(void);

// 时钟 tick 处理
void qos_tick(struct proc *p);

// 检查是否需要抢占
int qos_should_preempt(struct proc *current);

// 进程让出 CPU
void qos_yield(struct proc *p);

// I/O 完成通知
void qos_io_complete(struct proc *p);

// 获取 QoS 统计信息
void qos_get_stats(uint64 *total, uint64 *boosts, uint64 *misses, uint64 *switches);

// 打印 QoS 统计信息
void qos_print_stats(void);

// 打印进程的 QoS 信息
void qos_print_task(struct proc *p);

// 配置 QoS
void qos_configure(int enabled, int auto_boost, int deadline_aware, int io_boost);

// 获取 QoS 级别名称
char* qos_level_name(int level);

#endif // _QOS_H_
