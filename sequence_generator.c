/**
 * @file sequence_generator.c
 * @brief 页面访问序列生成器实现
 */

#include "sequence_generator.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// 生成随机序列
int* generate_random_sequence(int length, int max_page) {
    int* sequence = (int*)malloc(length * sizeof(int));
    srand((unsigned int)time(NULL));
    
    for (int i = 0; i < length; i++) {
        sequence[i] = rand() % max_page;
    }
    
    return sequence;
}

// 生成强局部性序列
int* generate_strong_locality_sequence(int length, int hot_pages, int total_pages) {
    int* sequence = (int*)malloc(length * sizeof(int));
    srand((unsigned int)time(NULL));
    
    // 80%的概率访问热点页面，20%的概率访问其他页面
    for (int i = 0; i < length; i++) {
        if (rand() % 10 < 8) {
            sequence[i] = rand() % hot_pages;
        } else {
            sequence[i] = hot_pages + rand() % (total_pages - hot_pages);
        }
    }
    
    return sequence;
}

// 生成弱局部性序列
int* generate_weak_locality_sequence(int length, int total_pages) {
    int* sequence = (int*)malloc(length * sizeof(int));
    srand((unsigned int)time(NULL));
    
    // 均匀随机访问所有页面
    for (int i = 0; i < length; i++) {
        sequence[i] = rand() % total_pages;
    }
    
    return sequence;
}

// 生成顺序访问序列
int* generate_sequential_sequence(int length, int start_page) {
    int* sequence = (int*)malloc(length * sizeof(int));
    
    for (int i = 0; i < length; i++) {
        sequence[i] = (start_page + i) % 100; // 假设最多100个页面
    }
    
    return sequence;
}

// 生成循环访问序列
int* generate_loop_sequence(int length, int* pages, int page_count) {
    if (pages == NULL || page_count == 0) {
        return NULL;
    }
    
    int* sequence = (int*)malloc(length * sizeof(int));
    
    for (int i = 0; i < length; i++) {
        sequence[i] = pages[i % page_count];
    }
    
    return sequence;
}

// 生成Belady异常测试序列
int* generate_belady_sequence(int length) {
    // 经典的Belady异常测试序列
    int belady_seq[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int belady_len = sizeof(belady_seq) / sizeof(belady_seq[0]);
    
    int* sequence = (int*)malloc(length * sizeof(int));
    
    for (int i = 0; i < length; i++) {
        sequence[i] = belady_seq[i % belady_len];
    }
    
    return sequence;
}

// 生成指定类型的序列
int* generate_sequence(SequenceType type, int length, ...) {
    va_list args;
    va_start(args, length);
    
    int* sequence = NULL;
    
    switch (type) {
        case SEQ_RANDOM: {
            int max_page = va_arg(args, int);
            sequence = generate_random_sequence(length, max_page);
            break;
        }
        case SEQ_LOCALITY_STRONG: {
            int hot_pages = va_arg(args, int);
            int total_pages = va_arg(args, int);
            sequence = generate_strong_locality_sequence(length, hot_pages, total_pages);
            break;
        }
        case SEQ_LOCALITY_WEAK: {
            int total_pages = va_arg(args, int);
            sequence = generate_weak_locality_sequence(length, total_pages);
            break;
        }
        case SEQ_SEQUENTIAL: {
            int start_page = va_arg(args, int);
            sequence = generate_sequential_sequence(length, start_page);
            break;
        }
        case SEQ_LOOP: {
            int* pages = va_arg(args, int*);
            int page_count = va_arg(args, int);
            sequence = generate_loop_sequence(length, pages, page_count);
            break;
        }
        case SEQ_BELADY: {
            sequence = generate_belady_sequence(length);
            break;
        }
        default:
            sequence = generate_random_sequence(length, 20);
    }
    
    va_end(args);
    return sequence;
}

// 保存序列到文件
void save_sequence_to_file(int* sequence, int length, const char* filename) {
    if (sequence == NULL || filename == NULL) return;
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("无法创建文件: %s\n", filename);
        return;
    }
    
    for (int i = 0; i < length; i++) {
        fprintf(file, "%d", sequence[i]);
        if (i < length - 1) {
            fprintf(file, " ");
        }
    }
    fprintf(file, "\n");
    
    fclose(file);
    printf("序列已保存到: %s\n", filename);
}

// 从文件加载序列
int* load_sequence_from_file(const char* filename, int* length) {
    if (filename == NULL || length == NULL) return NULL;
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("无法打开文件: %s\n", filename);
        *length = 0;
        return NULL;
    }
    
    // 先读取文件确定长度
    int count = 0;
    int temp;
    while (fscanf(file, "%d", &temp) == 1) {
        count++;
    }
    
    if (count == 0) {
        fclose(file);
        *length = 0;
        return NULL;
    }
    
    // 重新读取文件
    rewind(file);
    int* sequence = (int*)malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        fscanf(file, "%d", &sequence[i]);
    }
    
    fclose(file);
    *length = count;
    return sequence;
}

