/**
 * @file analyzer.h
 * @brief 性能分析器头文件
 * @details 提供算法性能分析、统计和可视化功能
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef ANALYZER_H
#define ANALYZER_H

#include "page_replacement.h"
#include <time.h>
#include <math.h>

// 性能分析结果
typedef struct {
    AlgorithmType algorithm;
    double avg_fault_rate;      // 平均缺页率
    double avg_hit_rate;        // 平均命中率
    double min_fault_rate;      // 最小缺页率
    double max_fault_rate;      // 最大缺页率
    double std_deviation;       // 标准差
    double avg_execution_time;  // 平均执行时间（微秒）
    int test_count;             // 测试次数
} PerformanceAnalysis;

// 执行步骤记录
typedef struct {
    int step;                   // 步骤编号
    int page_num;               // 访问的页面号
    int* frame_state;           // 当前帧状态（页面号数组）
    int frame_count;            // 帧数量
    bool is_fault;              // 是否缺页
    int replaced_page;          // 被替换的页面（-1表示未替换）
    int replaced_frame;         // 被替换的帧索引
    double timestamp;           // 时间戳
} ExecutionStep;

// 执行过程记录
typedef struct {
    AlgorithmType algorithm;
    int* page_sequence;         // 页面访问序列
    int seq_len;                // 序列长度
    int frame_count;            // 帧数量
    ExecutionStep* steps;       // 执行步骤数组
    int step_count;             // 步骤数量
    AlgorithmStats stats;       // 统计信息
} ExecutionTrace;

// 函数声明
// 性能分析
PerformanceAnalysis analyze_algorithm_performance(
    AlgorithmType algorithm,
    int** test_sequences,
    int* seq_lens,
    int test_count,
    int frame_count
);

// 执行过程追踪
ExecutionTrace* trace_algorithm_execution(
    AlgorithmType algorithm,
    int* page_sequence,
    int seq_len,
    int frame_count
);

// 释放执行追踪
void free_execution_trace(ExecutionTrace* trace);

// 打印执行过程
void print_execution_trace(ExecutionTrace* trace);

// 生成执行报告
void generate_execution_report(ExecutionTrace* trace, const char* filename);

// 性能对比分析
void compare_performance_analysis(
    PerformanceAnalysis* analyses,
    int count
);

// 统计分析
void statistical_analysis(
    AlgorithmResult* results,
    int count,
    const char* output_file
);

// 生成性能图表数据
void generate_chart_data(
    AlgorithmResult* results,
    int count,
    const char* output_file
);

#endif // ANALYZER_H

