/**
 * @file page_replacement.h
 * @brief 页面置换算法头文件
 * @details 定义了页面置换算法所需的数据结构和函数接口
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef PAGE_REPLACEMENT_H
#define PAGE_REPLACEMENT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/**
 * @brief 页面置换算法类型枚举
 * @details 定义了7种页面置换算法类型
 */
typedef enum {
    ALGORITHM_FIFO = 0,      // 先进先出
    ALGORITHM_LRU,           // 最近最少使用
    ALGORITHM_OPT,           // 最优算法
    ALGORITHM_CLOCK,          // 时钟算法
    ALGORITHM_LFU,           // 最不经常使用
    ALGORITHM_CLOCK_IMPROVED, // 改进的时钟算法
    ALGORITHM_PBA,            // 页面缓冲置换算法
    ALGORITHM_ADAPTIVE        // 自适应算法（创新点）
} AlgorithmType;

/**
 * @brief 页面帧结构体
 * @details 存储物理帧中的页面信息，包括页面号、访问时间、频率、引用位、修改位等
 */
typedef struct {
    int page_num;           // 页面号
    int access_time;        // 访问时间（用于LRU）
    int frequency;          // 访问频率（用于LFU）
    bool reference_bit;     // 引用位（用于Clock）
    bool modified_bit;      // 修改位（用于改进Clock）
} PageFrame;

/**
 * @brief 算法统计信息结构体
 * @details 记录算法执行过程中的性能指标
 */
typedef struct {
    int page_faults;        // 缺页次数
    int page_hits;          // 命中次数
    int total_accesses;     // 总访问次数
    double hit_rate;        // 命中率
    double fault_rate;      // 缺页率
} AlgorithmStats;

/**
 * @brief 算法结果结构体
 * @details 包含算法类型、统计信息和帧序列，用于结果返回和对比分析
 */
typedef struct {
    AlgorithmType algorithm;
    AlgorithmStats stats;
    int* frame_sequence;    // 帧序列（用于可视化）
    int frame_count;
} AlgorithmResult;

// 函数声明
// 初始化页面帧
void init_frames(PageFrame* frames, int frame_count);

// 各种页面置换算法实现
AlgorithmResult fifo_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult lru_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult opt_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult clock_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult lfu_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult clock_improved_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult pba_algorithm(int* page_sequence, int seq_len, int frame_count);
AlgorithmResult adaptive_algorithm(int* page_sequence, int seq_len, int frame_count);

// 工具函数
void print_result(AlgorithmResult result);
void compare_algorithms(AlgorithmResult* results, int count);
int* read_page_sequence(const char* filename, int* length);
const char* get_algorithm_name(AlgorithmType type);
void free_result(AlgorithmResult* result);
void print_execution_process(int* page_sequence, int seq_len, int frame_count, AlgorithmType type);
void visualize_algorithm(int* page_sequence, int seq_len, int frame_count, AlgorithmType type);

#endif // PAGE_REPLACEMENT_H
