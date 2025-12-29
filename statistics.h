/**
 * @file statistics.h
 * @brief 统计分析模块头文件
 * @details 提供算法性能的统计分析功能
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include "page_replacement.h"
#include <math.h>

// 统计结果
typedef struct {
    double mean;                // 平均值
    double median;              // 中位数
    double variance;            // 方差
    double std_deviation;       // 标准差
    double min_value;           // 最小值
    double max_value;           // 最大值
    int count;                  // 样本数量
} Statistics;

// 函数声明
// 计算统计信息
Statistics calculate_statistics(double* data, int count);

// 打印统计信息
void print_statistics(Statistics* stats, const char* label);

// 算法性能统计
void algorithm_statistics(
    AlgorithmResult* results,
    int count,
    Statistics* fault_rate_stats,
    Statistics* hit_rate_stats
);

#endif // STATISTICS_H

