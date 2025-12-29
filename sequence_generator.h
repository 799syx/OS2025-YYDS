/**
 * @file sequence_generator.h
 * @brief 页面访问序列生成器
 * @details 生成各种类型的页面访问序列用于测试
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef SEQUENCE_GENERATOR_H
#define SEQUENCE_GENERATOR_H

#include <stdlib.h>
#include <stdarg.h>

// 序列类型
typedef enum {
    SEQ_RANDOM,          // 随机序列
    SEQ_LOCALITY_STRONG, // 强局部性序列
    SEQ_LOCALITY_WEAK,   // 弱局部性序列
    SEQ_SEQUENTIAL,      // 顺序访问序列
    SEQ_LOOP,            // 循环访问序列
    SEQ_BELADY           // Belady异常测试序列
} SequenceType;

// 函数声明
// 生成随机序列
int* generate_random_sequence(int length, int max_page);

// 生成强局部性序列
int* generate_strong_locality_sequence(int length, int hot_pages, int total_pages);

// 生成弱局部性序列
int* generate_weak_locality_sequence(int length, int total_pages);

// 生成顺序访问序列
int* generate_sequential_sequence(int length, int start_page);

// 生成循环访问序列
int* generate_loop_sequence(int length, int* pages, int page_count);

// 生成Belady异常测试序列
int* generate_belady_sequence(int length);

// 生成指定类型的序列
int* generate_sequence(SequenceType type, int length, ...);

// 保存序列到文件
void save_sequence_to_file(int* sequence, int length, const char* filename);

// 从文件加载序列
int* load_sequence_from_file(const char* filename, int* length);

#endif // SEQUENCE_GENERATOR_H

