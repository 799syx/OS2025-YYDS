/**
 * @file performance_monitor.c
 * @brief 性能监控器实现
 */

#include "performance_monitor.h"
#include "page_replacement.h"
#include <stdio.h>
#include <stdlib.h>

// 初始化性能监控
PerformanceMonitor* monitor_init(AlgorithmType algorithm) {
    PerformanceMonitor* monitor = (PerformanceMonitor*)malloc(sizeof(PerformanceMonitor));
    monitor->algorithm = algorithm;
    monitor->start_time = 0;
    monitor->end_time = 0;
    monitor->memory_usage = 0;
    monitor->function_calls = 0;
    monitor->cpu_usage = 0.0;
    return monitor;
}

// 开始监控
void monitor_start(PerformanceMonitor* monitor) {
    if (monitor == NULL) return;
    monitor->start_time = clock();
}

// 结束监控
void monitor_end(PerformanceMonitor* monitor) {
    if (monitor == NULL) return;
    monitor->end_time = clock();
}

// 获取执行时间（微秒）
double monitor_get_execution_time(PerformanceMonitor* monitor) {
    if (monitor == NULL) return 0.0;
    return ((double)(monitor->end_time - monitor->start_time)) / CLOCKS_PER_SEC * 1000000;
}

// 打印监控结果
void monitor_print_result(PerformanceMonitor* monitor) {
    if (monitor == NULL) return;
    
    printf("\n========== 性能监控结果 ==========\n");
    printf("算法: %s\n", get_algorithm_name(monitor->algorithm));
    printf("执行时间: %.2f 微秒\n", monitor_get_execution_time(monitor));
    printf("内存使用: %ld 字节\n", monitor->memory_usage);
    printf("函数调用次数: %d\n", monitor->function_calls);
    printf("CPU使用率: %.2f%%\n", monitor->cpu_usage);
    printf("==================================\n");
}

// 释放监控器
void monitor_free(PerformanceMonitor* monitor) {
    if (monitor != NULL) {
        free(monitor);
    }
}

