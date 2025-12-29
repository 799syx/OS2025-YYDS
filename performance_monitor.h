/**
 * @file performance_monitor.h
 * @brief 性能监控器头文件
 * @details 实时监控算法执行性能
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include "page_replacement.h"
#include <time.h>

// 性能监控数据
typedef struct {
    AlgorithmType algorithm;
    clock_t start_time;
    clock_t end_time;
    long memory_usage;          // 内存使用量（字节）
    int function_calls;         // 函数调用次数
    double cpu_usage;           // CPU使用率
} PerformanceMonitor;

// 函数声明
// 初始化性能监控
PerformanceMonitor* monitor_init(AlgorithmType algorithm);

// 开始监控
void monitor_start(PerformanceMonitor* monitor);

// 结束监控
void monitor_end(PerformanceMonitor* monitor);

// 获取执行时间（微秒）
double monitor_get_execution_time(PerformanceMonitor* monitor);

// 打印监控结果
void monitor_print_result(PerformanceMonitor* monitor);

// 释放监控器
void monitor_free(PerformanceMonitor* monitor);

#endif // PERFORMANCE_MONITOR_H

