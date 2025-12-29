/**
 * @file statistics.c
 * @brief 统计分析模块实现
 */

#include "statistics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 比较函数（用于排序）
static int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

// 计算统计信息
Statistics calculate_statistics(double* data, int count) {
    Statistics stats;
    stats.count = count;
    
    if (count == 0) {
        stats.mean = 0.0;
        stats.median = 0.0;
        stats.variance = 0.0;
        stats.std_deviation = 0.0;
        stats.min_value = 0.0;
        stats.max_value = 0.0;
        return stats;
    }
    
    // 计算平均值
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += data[i];
    }
    stats.mean = sum / count;
    
    // 计算最小值和最大值
    stats.min_value = data[0];
    stats.max_value = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] < stats.min_value) {
            stats.min_value = data[i];
        }
        if (data[i] > stats.max_value) {
            stats.max_value = data[i];
        }
    }
    
    // 计算方差和标准差
    double variance_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = data[i] - stats.mean;
        variance_sum += diff * diff;
    }
    stats.variance = variance_sum / count;
    stats.std_deviation = sqrt(stats.variance);
    
    // 计算中位数
    double* sorted_data = (double*)malloc(count * sizeof(double));
    memcpy(sorted_data, data, count * sizeof(double));
    qsort(sorted_data, count, sizeof(double), compare_double);
    
    if (count % 2 == 0) {
        stats.median = (sorted_data[count / 2 - 1] + sorted_data[count / 2]) / 2.0;
    } else {
        stats.median = sorted_data[count / 2];
    }
    
    free(sorted_data);
    return stats;
}

// 打印统计信息
void print_statistics(Statistics* stats, const char* label) {
    if (stats == NULL) return;
    
    printf("\n========== %s 统计信息 ==========\n", label);
    printf("样本数量: %d\n", stats->count);
    printf("平均值: %.4f\n", stats->mean);
    printf("中位数: %.4f\n", stats->median);
    printf("最小值: %.4f\n", stats->min_value);
    printf("最大值: %.4f\n", stats->max_value);
    printf("方差: %.4f\n", stats->variance);
    printf("标准差: %.4f\n", stats->std_deviation);
    printf("==================================\n");
}

// 算法性能统计
void algorithm_statistics(
    AlgorithmResult* results,
    int count,
    Statistics* fault_rate_stats,
    Statistics* hit_rate_stats
) {
    if (results == NULL || count == 0) return;
    
    double* fault_rates = (double*)malloc(count * sizeof(double));
    double* hit_rates = (double*)malloc(count * sizeof(double));
    
    for (int i = 0; i < count; i++) {
        fault_rates[i] = results[i].stats.fault_rate;
        hit_rates[i] = results[i].stats.hit_rate;
    }
    
    if (fault_rate_stats != NULL) {
        *fault_rate_stats = calculate_statistics(fault_rates, count);
    }
    
    if (hit_rate_stats != NULL) {
        *hit_rate_stats = calculate_statistics(hit_rates, count);
    }
    
    free(fault_rates);
    free(hit_rates);
}

