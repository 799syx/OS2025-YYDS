/**
 * @file comprehensive_test.c
 * @brief 综合测试程序
 * @details 包含多种测试场景和性能分析
 */

#include "page_replacement.h"
#include "analyzer.h"
#include "sequence_generator.h"
#include "logger.h"
#include <time.h>
#include <stdlib.h>

// 执行算法（统一接口）
AlgorithmResult execute_algorithm(
    AlgorithmType algorithm,
    int* sequence,
    int seq_len,
    int frame_count
);

// 综合测试：多种序列类型
void comprehensive_test() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                   综合测试程序                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // 初始化日志
    logger_init(LOG_INFO, false, true, NULL);
    
    int frame_count = 4;
    AlgorithmType algorithms[] = {
        ALGORITHM_FIFO,
        ALGORITHM_LRU,
        ALGORITHM_OPT,
        ALGORITHM_CLOCK,
        ALGORITHM_LFU,
        ALGORITHM_CLOCK_IMPROVED,
        ALGORITHM_PBA,
        ALGORITHM_ADAPTIVE
    };
    int alg_count = sizeof(algorithms) / sizeof(algorithms[0]);
    
    // 测试1：随机序列
    printf("\n========== 测试1: 随机序列 ==========\n");
    int* random_seq = generate_random_sequence(100, 20);
    printf("序列长度: 100, 页面范围: 0-19\n");
    
    AlgorithmResult results1[8];
    for (int i = 0; i < alg_count; i++) {
        results1[i] = execute_algorithm(algorithms[i], random_seq, 100, frame_count);
    }
    compare_algorithms(results1, alg_count);
    
    for (int i = 0; i < alg_count; i++) {
        free_result(&results1[i]);
    }
    free(random_seq);
    
    // 测试2：强局部性序列
    printf("\n========== 测试2: 强局部性序列 ==========\n");
    int* locality_seq = generate_strong_locality_sequence(100, 5, 20);
    printf("序列长度: 100, 热点页面: 5个, 总页面: 20个\n");
    
    AlgorithmResult results2[8];
    for (int i = 0; i < alg_count; i++) {
        results2[i] = execute_algorithm(algorithms[i], locality_seq, 100, frame_count);
    }
    compare_algorithms(results2, alg_count);
    
    for (int i = 0; i < alg_count; i++) {
        free_result(&results2[i]);
    }
    free(locality_seq);
    
    // 测试3：执行过程追踪
    printf("\n========== 测试3: 执行过程追踪 (FIFO算法) ==========\n");
    int test_seq[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    int test_len = sizeof(test_seq) / sizeof(test_seq[0]);
    
    ExecutionTrace* trace = trace_algorithm_execution(
        ALGORITHM_FIFO, test_seq, test_len, 3
    );
    print_execution_trace(trace);
    generate_execution_report(trace, "execution_report.txt");
    free_execution_trace(trace);
    
    // 测试4：性能分析
    printf("\n========== 测试4: 性能分析 ==========\n");
    int** test_seqs = (int**)malloc(5 * sizeof(int*));
    int seq_lens[5] = {50, 50, 50, 50, 50};
    
    for (int i = 0; i < 5; i++) {
        test_seqs[i] = generate_random_sequence(50, 15);
    }
    
    PerformanceAnalysis analyses[3];
    analyses[0] = analyze_algorithm_performance(
        ALGORITHM_FIFO, test_seqs, seq_lens, 5, frame_count
    );
    analyses[1] = analyze_algorithm_performance(
        ALGORITHM_LRU, test_seqs, seq_lens, 5, frame_count
    );
    analyses[2] = analyze_algorithm_performance(
        ALGORITHM_ADAPTIVE, test_seqs, seq_lens, 5, frame_count
    );
    
    compare_performance_analysis(analyses, 3);
    
    for (int i = 0; i < 5; i++) {
        free(test_seqs[i]);
    }
    free(test_seqs);
    
    logger_close();
}

// 执行算法（统一接口）实现
AlgorithmResult execute_algorithm(
    AlgorithmType algorithm,
    int* sequence,
    int seq_len,
    int frame_count
) {
    switch (algorithm) {
        case ALGORITHM_FIFO:
            return fifo_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_LRU:
            return lru_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_OPT:
            return opt_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_CLOCK:
            return clock_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_LFU:
            return lfu_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_CLOCK_IMPROVED:
            return clock_improved_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_PBA:
            return pba_algorithm(sequence, seq_len, frame_count);
        case ALGORITHM_ADAPTIVE:
            return adaptive_algorithm(sequence, seq_len, frame_count);
        default: {
            AlgorithmResult empty;
            empty.stats.page_faults = 0;
            empty.stats.page_hits = 0;
            return empty;
        }
    }
}

int main() {
    comprehensive_test();
    printf("\n综合测试完成！\n");
    return 0;
}

