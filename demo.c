/**
 * @file demo.c
 * @brief 演示程序 - 用于比赛演示
 * @details 展示各种算法的执行过程和性能对比
 */

#include "page_replacement.h"

// 演示用例1：经典序列
void demo_classic_sequence() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           演示用例1：经典测试序列                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    int sequence[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    int seq_len = sizeof(sequence) / sizeof(sequence[0]);
    int frame_count = 3;
    
    printf("\n页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%d ", sequence[i]);
    }
    printf("\n物理帧数: %d\n", frame_count);
    
    // 展示FIFO算法的执行过程
    printf("\n--- FIFO算法执行过程 ---\n");
    print_execution_process(sequence, seq_len, frame_count, ALGORITHM_FIFO);
    
    // 对比所有算法
    printf("\n--- 所有算法性能对比 ---\n");
    AlgorithmResult results[8];
    results[0] = fifo_algorithm(sequence, seq_len, frame_count);
    results[1] = lru_algorithm(sequence, seq_len, frame_count);
    results[2] = opt_algorithm(sequence, seq_len, frame_count);
    results[3] = clock_algorithm(sequence, seq_len, frame_count);
    results[4] = lfu_algorithm(sequence, seq_len, frame_count);
    results[5] = clock_improved_algorithm(sequence, seq_len, frame_count);
    results[6] = pba_algorithm(sequence, seq_len, frame_count);
    results[7] = adaptive_algorithm(sequence, seq_len, frame_count);
    
    compare_algorithms(results, 8);
    
    for (int i = 0; i < 8; i++) {
        free_result(&results[i]);
    }
}

// 演示用例2：Belady异常
void demo_belady_anomaly() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           演示用例2：Belady异常演示                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    int sequence[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int seq_len = sizeof(sequence) / sizeof(sequence[0]);
    
    printf("\n页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%d ", sequence[i]);
    }
    printf("\n");
    
    int frame_counts[] = {3, 4};
    
    for (int f = 0; f < 2; f++) {
        printf("\n--- 物理帧数: %d ---\n", frame_counts[f]);
        AlgorithmResult fifo = fifo_algorithm(sequence, seq_len, frame_counts[f]);
        AlgorithmResult lru = lru_algorithm(sequence, seq_len, frame_counts[f]);
        
        printf("FIFO算法: ");
        print_result(fifo);
        printf("LRU算法: ");
        print_result(lru);
        
        if (f == 0) {
            printf("\n注意：FIFO算法在帧数增加时，缺页次数可能增加（Belady异常）\n");
        }
        
        free_result(&fifo);
        free_result(&lru);
    }
}

// 演示用例3：自适应算法
void demo_adaptive() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           演示用例3：自适应算法演示                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // 强局部性序列
    int sequence1[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
    int seq_len1 = sizeof(sequence1) / sizeof(sequence1[0]);
    
    printf("\n--- 强局部性序列 ---\n");
    printf("页面访问序列: ");
    for (int i = 0; i < seq_len1; i++) {
        printf("%d ", sequence1[i]);
    }
    printf("\n");
    
    AlgorithmResult adaptive1 = adaptive_algorithm(sequence1, seq_len1, 3);
    AlgorithmResult lru1 = lru_algorithm(sequence1, seq_len1, 3);
    
    printf("自适应算法: ");
    print_result(adaptive1);
    printf("LRU算法（自适应算法可能选择）: ");
    print_result(lru1);
    
    free_result(&adaptive1);
    free_result(&lru1);
}

int main() {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║        页面置换算法演示程序 - 比赛演示版本                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    demo_classic_sequence();
    demo_belady_anomaly();
    demo_adaptive();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    演示完成                                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

