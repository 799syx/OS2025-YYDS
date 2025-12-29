#include "page_replacement.h"

// 获取算法名称
const char* get_algorithm_name(AlgorithmType type) {
    switch (type) {
        case ALGORITHM_FIFO:
            return "FIFO (先进先出)";
        case ALGORITHM_LRU:
            return "LRU (最近最少使用)";
        case ALGORITHM_OPT:
            return "OPT (最优算法)";
        case ALGORITHM_CLOCK:
            return "Clock (时钟算法)";
        case ALGORITHM_LFU:
            return "LFU (最不经常使用)";
        case ALGORITHM_CLOCK_IMPROVED:
            return "Clock-Improved (改进时钟算法)";
        case ALGORITHM_PBA:
            return "PBA (页面缓冲置换算法)";
        case ALGORITHM_ADAPTIVE:
            return "Adaptive (自适应算法)";
        default:
            return "Unknown";
    }
}

// 打印算法结果
void print_result(AlgorithmResult result) {
    printf("\n========== %s ==========\n", get_algorithm_name(result.algorithm));
    printf("总访问次数: %d\n", result.stats.total_accesses);
    printf("页面命中次数: %d\n", result.stats.page_hits);
    printf("缺页次数: %d\n", result.stats.page_faults);
    printf("命中率: %.2f%%\n", result.stats.hit_rate * 100);
    printf("缺页率: %.2f%%\n", result.stats.fault_rate * 100);
    printf("==========================\n");
}

// 比较多个算法的性能
void compare_algorithms(AlgorithmResult* results, int count) {
    printf("\n\n========== 算法性能对比 ==========\n");
    printf("%-25s %10s %10s %12s %12s\n", 
           "算法名称", "缺页次数", "命中次数", "命中率", "缺页率");
    printf("------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        printf("%-25s %10d %10d %11.2f%% %11.2f%%\n",
               get_algorithm_name(results[i].algorithm),
               results[i].stats.page_faults,
               results[i].stats.page_hits,
               results[i].stats.hit_rate * 100,
               results[i].stats.fault_rate * 100);
    }
    printf("==========================================\n\n");
    
    // 找出最优算法
    int best_idx = 0;
    for (int i = 1; i < count; i++) {
        if (results[i].stats.page_faults < results[best_idx].stats.page_faults) {
            best_idx = i;
        }
    }
    
    printf("最优算法: %s (缺页次数最少: %d)\n\n", 
           get_algorithm_name(results[best_idx].algorithm),
           results[best_idx].stats.page_faults);
}

// 从文件读取页面访问序列
int* read_page_sequence(const char* filename, int* length) {
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

// 释放结果内存
void free_result(AlgorithmResult* result) {
    if (result->frame_sequence != NULL) {
        free(result->frame_sequence);
        result->frame_sequence = NULL;
    }
}

// 辅助函数（用于可视化）
static int find_page_in_frames_helper(PageFrame* frames, int frame_count, int page_num) {
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].page_num == page_num) {
            return i;
        }
    }
    return -1;
}

static int find_free_frame_helper(PageFrame* frames, int frame_count) {
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].page_num == -1) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 打印算法执行过程（简化版可视化）
 * @param page_sequence 页面访问序列
 * @param seq_len 序列长度
 * @param frame_count 物理帧数
 * @param type 算法类型
 */
void print_execution_process(int* page_sequence, int seq_len, int frame_count, AlgorithmType type) {
    AlgorithmResult result;
    
    switch (type) {
        case ALGORITHM_FIFO:
            result = fifo_algorithm(page_sequence, seq_len, frame_count);
            break;
        case ALGORITHM_LRU:
            result = lru_algorithm(page_sequence, seq_len, frame_count);
            break;
        case ALGORITHM_OPT:
            result = opt_algorithm(page_sequence, seq_len, frame_count);
            break;
        default:
            printf("该算法暂不支持详细过程输出\n");
            return;
    }
    
    printf("\n========== %s 执行过程 ==========\n", get_algorithm_name(type));
    printf("页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%2d ", page_sequence[i]);
    }
    printf("\n物理帧数: %d\n", frame_count);
    
    // 简化的执行过程展示
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    for (int i = 0; i < frame_count; i++) {
        frames[i].page_num = -1;
        frames[i].access_time = -1;
    }
    
    int next_replace = 0;
    int fault_count = 0;
    
    printf("\n步骤 | 访问页面 | ");
    for (int i = 0; i < frame_count; i++) {
        printf("帧%d | ", i);
    }
    printf("缺页\n");
    printf("-----|---------|");
    for (int i = 0; i < frame_count; i++) {
        printf("-----|");
    }
    printf("-----\n");
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        bool page_fault = false;
        
        int frame_idx = find_page_in_frames_helper(frames, frame_count, page_num);
        
        if (frame_idx == -1) {
            page_fault = true;
            fault_count++;
            int free_idx = find_free_frame_helper(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                frames[free_idx].access_time = i;
            } else {
                frames[next_replace].page_num = page_num;
                frames[next_replace].access_time = i;
                next_replace = (next_replace + 1) % frame_count;
            }
        } else {
            frames[frame_idx].access_time = i;
        }
        
        printf("%4d | %7d | ", i + 1, page_num);
        for (int j = 0; j < frame_count; j++) {
            if (frames[j].page_num == -1) {
                printf("  - | ");
            } else {
                printf("%3d | ", frames[j].page_num);
            }
        }
        printf("%s\n", page_fault ? "是" : "否");
    }
    
    free(frames);
    printf("\n总缺页次数: %d\n", fault_count);
    print_result(result);
    free_result(&result);
}

/**
 * @brief 可视化算法执行（ASCII图表）
 * @param page_sequence 页面访问序列
 * @param seq_len 序列长度
 * @param frame_count 物理帧数
 * @param type 算法类型
 */
void visualize_algorithm(int* page_sequence, int seq_len, int frame_count, AlgorithmType type) {
    printf("\n========== %s 可视化 ==========\n", get_algorithm_name(type));
    printf("页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%d ", page_sequence[i]);
    }
    printf("\n物理帧数: %d\n", frame_count);
    printf("\n(详细可视化功能需要根据具体算法实现)\n");
    
    AlgorithmResult result;
    switch (type) {
        case ALGORITHM_FIFO:
            result = fifo_algorithm(page_sequence, seq_len, frame_count);
            break;
        case ALGORITHM_LRU:
            result = lru_algorithm(page_sequence, seq_len, frame_count);
            break;
        case ALGORITHM_OPT:
            result = opt_algorithm(page_sequence, seq_len, frame_count);
            break;
        default:
            return;
    }
    
    print_result(result);
    free_result(&result);
}

