#include "page_replacement.h"

// 测试用例1：经典测试序列
void test_case_1() {
    printf("\n========== 测试用例1: 经典序列 ==========\n");
    int sequence[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    int seq_len = sizeof(sequence) / sizeof(sequence[0]);
    int frame_count = 3;
    
    printf("页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%d ", sequence[i]);
    }
    printf("\n物理帧数: %d\n", frame_count);
    
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
    
    // 释放内存
    for (int i = 0; i < 8; i++) {
        free_result(&results[i]);
    }
}

// 测试用例2：Belady异常测试（FIFO会出现异常）
void test_case_2() {
    printf("\n========== 测试用例2: Belady异常测试 ==========\n");
    int sequence[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    int seq_len = sizeof(sequence) / sizeof(sequence[0]);
    
    printf("页面访问序列: ");
    for (int i = 0; i < seq_len; i++) {
        printf("%d ", sequence[i]);
    }
    printf("\n");
    
    // 测试不同帧数
    int frame_counts[] = {3, 4};
    int num_frames = sizeof(frame_counts) / sizeof(frame_counts[0]);
    
    for (int f = 0; f < num_frames; f++) {
        printf("\n--- 物理帧数: %d ---\n", frame_counts[f]);
        AlgorithmResult fifo = fifo_algorithm(sequence, seq_len, frame_counts[f]);
        AlgorithmResult lru = lru_algorithm(sequence, seq_len, frame_counts[f]);
        
        print_result(fifo);
        print_result(lru);
        
        free_result(&fifo);
        free_result(&lru);
    }
}

// 测试用例3：局部性访问模式
void test_case_3() {
    printf("\n========== 测试用例3: 局部性访问模式 ==========\n");
    // 生成具有局部性的序列
    int sequence[50];
    srand(time(NULL));
    for (int i = 0; i < 50; i++) {
        // 80%的概率访问前5个页面，20%的概率访问其他页面
        if (rand() % 10 < 8) {
            sequence[i] = rand() % 5;
        } else {
            sequence[i] = 5 + rand() % 10;
        }
    }
    
    printf("页面访问序列: ");
    for (int i = 0; i < 50; i++) {
        printf("%d ", sequence[i]);
    }
    printf("\n物理帧数: 4\n");
    
    AlgorithmResult results[3];
    results[0] = fifo_algorithm(sequence, 50, 4);
    results[1] = lru_algorithm(sequence, 50, 4);
    results[2] = adaptive_algorithm(sequence, 50, 4);
    
    compare_algorithms(results, 3);
    
    for (int i = 0; i < 3; i++) {
        free_result(&results[i]);
    }
}

int main() {
    printf("==========================================\n");
    printf("    页面置换算法测试程序\n");
    printf("==========================================\n");
    
    test_case_1();
    test_case_2();
    test_case_3();
    
    printf("\n测试完成！\n");
    return 0;
}

