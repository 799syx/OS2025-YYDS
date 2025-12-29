/**
 * @file analyzer.c
 * @brief 性能分析器实现
 * @details 提供算法性能分析、统计和可视化功能
 */

#include "analyzer.h"
#include <math.h>
#include <string.h>

// 内部函数：执行算法并记录步骤
static AlgorithmResult execute_with_trace(
    AlgorithmType algorithm,
    int* page_sequence,
    int seq_len,
    int frame_count,
    ExecutionStep** steps_out,
    int* step_count_out
) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    ExecutionStep* steps = (ExecutionStep*)malloc(seq_len * sizeof(ExecutionStep));
    int step_count = 0;
    
    AlgorithmResult result;
    result.algorithm = algorithm;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    // 根据算法类型执行（简化版，主要记录FIFO）
    int next_replace = 0;
    clock_t start = clock();
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        ExecutionStep step;
        step.step = i + 1;
        step.page_num = page_num;
        step.frame_state = (int*)malloc(frame_count * sizeof(int));
        step.frame_count = frame_count;
        step.is_fault = false;
        step.replaced_page = -1;
        step.replaced_frame = -1;
        step.timestamp = ((double)(clock() - start)) / CLOCKS_PER_SEC * 1000; // 毫秒
        
        // 记录当前帧状态
        for (int j = 0; j < frame_count; j++) {
            step.frame_state[j] = frames[j].page_num;
        }
        
        // 查找页面
        int frame_idx = -1;
        for (int j = 0; j < frame_count; j++) {
            if (frames[j].page_num == page_num) {
                frame_idx = j;
                break;
            }
        }
        
        if (frame_idx != -1) {
            // 页面命中
            result.stats.page_hits++;
            step.is_fault = false;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失
            result.stats.page_faults++;
            step.is_fault = true;
            
            int free_idx = -1;
            for (int j = 0; j < frame_count; j++) {
                if (frames[j].page_num == -1) {
                    free_idx = j;
                    break;
                }
            }
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                result.frame_sequence[i] = free_idx;
            } else {
                // 需要替换
                step.replaced_page = frames[next_replace].page_num;
                step.replaced_frame = next_replace;
                frames[next_replace].page_num = page_num;
                result.frame_sequence[i] = next_replace;
                next_replace = (next_replace + 1) % frame_count;
            }
        }
        
        steps[step_count++] = step;
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    *steps_out = steps;
    *step_count_out = step_count;
    
    free(frames);
    return result;
}

// 性能分析
PerformanceAnalysis analyze_algorithm_performance(
    AlgorithmType algorithm,
    int** test_sequences,
    int* seq_lens,
    int test_count,
    int frame_count
) {
    PerformanceAnalysis analysis;
    analysis.algorithm = algorithm;
    analysis.test_count = test_count;
    
    double* fault_rates = (double*)malloc(test_count * sizeof(double));
    double* hit_rates = (double*)malloc(test_count * sizeof(double));
    double* exec_times = (double*)malloc(test_count * sizeof(double));
    
    double sum_fault_rate = 0.0;
    double sum_hit_rate = 0.0;
    double sum_exec_time = 0.0;
    
    for (int i = 0; i < test_count; i++) {
        clock_t start = clock();
        AlgorithmResult result;
        
        switch (algorithm) {
            case ALGORITHM_FIFO:
                result = fifo_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_LRU:
                result = lru_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_OPT:
                result = opt_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_CLOCK:
                result = clock_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_LFU:
                result = lfu_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_CLOCK_IMPROVED:
                result = clock_improved_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_PBA:
                result = pba_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            case ALGORITHM_ADAPTIVE:
                result = adaptive_algorithm(test_sequences[i], seq_lens[i], frame_count);
                break;
            default:
                result.stats.fault_rate = 0.0;
                result.stats.hit_rate = 0.0;
        }
        
        clock_t end = clock();
        double exec_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000000; // 微秒
        
        fault_rates[i] = result.stats.fault_rate;
        hit_rates[i] = result.stats.hit_rate;
        exec_times[i] = exec_time;
        
        sum_fault_rate += fault_rates[i];
        sum_hit_rate += hit_rates[i];
        sum_exec_time += exec_times[i];
        
        free_result(&result);
    }
    
    // 计算平均值
    analysis.avg_fault_rate = sum_fault_rate / test_count;
    analysis.avg_hit_rate = sum_hit_rate / test_count;
    analysis.avg_execution_time = sum_exec_time / test_count;
    
    // 计算最小值和最大值
    analysis.min_fault_rate = fault_rates[0];
    analysis.max_fault_rate = fault_rates[0];
    for (int i = 1; i < test_count; i++) {
        if (fault_rates[i] < analysis.min_fault_rate) {
            analysis.min_fault_rate = fault_rates[i];
        }
        if (fault_rates[i] > analysis.max_fault_rate) {
            analysis.max_fault_rate = fault_rates[i];
        }
    }
    
    // 计算标准差
    double variance = 0.0;
    for (int i = 0; i < test_count; i++) {
        double diff = fault_rates[i] - analysis.avg_fault_rate;
        variance += diff * diff;
    }
    analysis.std_deviation = sqrt(variance / test_count);
    
    free(fault_rates);
    free(hit_rates);
    free(exec_times);
    
    return analysis;
}

// 执行过程追踪
ExecutionTrace* trace_algorithm_execution(
    AlgorithmType algorithm,
    int* page_sequence,
    int seq_len,
    int frame_count
) {
    ExecutionTrace* trace = (ExecutionTrace*)malloc(sizeof(ExecutionTrace));
    trace->algorithm = algorithm;
    trace->seq_len = seq_len;
    trace->frame_count = frame_count;
    
    trace->page_sequence = (int*)malloc(seq_len * sizeof(int));
    memcpy(trace->page_sequence, page_sequence, seq_len * sizeof(int));
    
    ExecutionStep* steps = NULL;
    int step_count = 0;
    
    AlgorithmResult result = execute_with_trace(
        algorithm, page_sequence, seq_len, frame_count,
        &steps, &step_count
    );
    
    trace->steps = steps;
    trace->step_count = step_count;
    trace->stats = result.stats;
    
    free(result.frame_sequence);
    
    return trace;
}

// 释放执行追踪
void free_execution_trace(ExecutionTrace* trace) {
    if (trace == NULL) return;
    
    if (trace->page_sequence != NULL) {
        free(trace->page_sequence);
    }
    
    if (trace->steps != NULL) {
        for (int i = 0; i < trace->step_count; i++) {
            if (trace->steps[i].frame_state != NULL) {
                free(trace->steps[i].frame_state);
            }
        }
        free(trace->steps);
    }
    
    free(trace);
}

// 打印执行过程
void print_execution_trace(ExecutionTrace* trace) {
    if (trace == NULL) return;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║        %s 执行过程追踪                    ║\n", get_algorithm_name(trace->algorithm));
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    printf("\n页面访问序列: ");
    for (int i = 0; i < trace->seq_len; i++) {
        printf("%d ", trace->page_sequence[i]);
    }
    printf("\n物理帧数: %d\n", trace->frame_count);
    
    printf("\n步骤 | 访问页面 | ");
    for (int i = 0; i < trace->frame_count; i++) {
        printf("帧%d | ", i);
    }
    printf("缺页 | 替换页面 | 替换帧\n");
    printf("-----|---------|");
    for (int i = 0; i < trace->frame_count; i++) {
        printf("-----|");
    }
    printf("-----|---------|-------\n");
    
    for (int i = 0; i < trace->step_count; i++) {
        ExecutionStep* step = &trace->steps[i];
        printf("%4d | %7d | ", step->step, step->page_num);
        
        for (int j = 0; j < trace->frame_count; j++) {
            if (step->frame_state[j] == -1) {
                printf("  - | ");
            } else {
                printf("%3d | ", step->frame_state[j]);
            }
        }
        
        printf("%s | ", step->is_fault ? "是" : "否");
        if (step->replaced_page != -1) {
            printf("%7d | %6d\n", step->replaced_page, step->replaced_frame);
        } else {
            printf("     - |      -\n");
        }
    }
    
    printf("\n统计信息:\n");
    printf("  总访问次数: %d\n", trace->stats.total_accesses);
    printf("  页面命中次数: %d\n", trace->stats.page_hits);
    printf("  缺页次数: %d\n", trace->stats.page_faults);
    printf("  命中率: %.2f%%\n", trace->stats.hit_rate * 100);
    printf("  缺页率: %.2f%%\n", trace->stats.fault_rate * 100);
}

// 生成执行报告
void generate_execution_report(ExecutionTrace* trace, const char* filename) {
    if (trace == NULL || filename == NULL) return;
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("无法创建报告文件: %s\n", filename);
        return;
    }
    
    fprintf(file, "页面置换算法执行报告\n");
    fprintf(file, "====================\n\n");
    fprintf(file, "算法: %s\n", get_algorithm_name(trace->algorithm));
    fprintf(file, "物理帧数: %d\n", trace->frame_count);
    fprintf(file, "序列长度: %d\n", trace->seq_len);
    fprintf(file, "\n页面访问序列: ");
    for (int i = 0; i < trace->seq_len; i++) {
        fprintf(file, "%d ", trace->page_sequence[i]);
    }
    fprintf(file, "\n\n");
    
    fprintf(file, "执行步骤:\n");
    fprintf(file, "步骤,访问页面,");
    for (int i = 0; i < trace->frame_count; i++) {
        fprintf(file, "帧%d,", i);
    }
    fprintf(file, "缺页,替换页面,替换帧\n");
    
    for (int i = 0; i < trace->step_count; i++) {
        ExecutionStep* step = &trace->steps[i];
        fprintf(file, "%d,%d,", step->step, step->page_num);
        
        for (int j = 0; j < trace->frame_count; j++) {
            if (step->frame_state[j] == -1) {
                fprintf(file, "-,");
            } else {
                fprintf(file, "%d,", step->frame_state[j]);
            }
        }
        
        fprintf(file, "%s,", step->is_fault ? "是" : "否");
        if (step->replaced_page != -1) {
            fprintf(file, "%d,%d\n", step->replaced_page, step->replaced_frame);
        } else {
            fprintf(file, "-,-\n");
        }
    }
    
    fprintf(file, "\n统计信息:\n");
    fprintf(file, "总访问次数: %d\n", trace->stats.total_accesses);
    fprintf(file, "页面命中次数: %d\n", trace->stats.page_hits);
    fprintf(file, "缺页次数: %d\n", trace->stats.page_faults);
    fprintf(file, "命中率: %.2f%%\n", trace->stats.hit_rate * 100);
    fprintf(file, "缺页率: %.2f%%\n", trace->stats.fault_rate * 100);
    
    fclose(file);
    printf("执行报告已保存到: %s\n", filename);
}

// 性能对比分析
void compare_performance_analysis(
    PerformanceAnalysis* analyses,
    int count
) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        性能分析对比报告                                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    
    printf("\n%-25s %12s %12s %12s %12s %15s\n",
           "算法名称", "平均缺页率", "最小缺页率", "最大缺页率", "标准差", "平均执行时间(μs)");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        printf("%-25s %11.2f%% %11.2f%% %11.2f%% %11.4f %14.2f\n",
               get_algorithm_name(analyses[i].algorithm),
               analyses[i].avg_fault_rate * 100,
               analyses[i].min_fault_rate * 100,
               analyses[i].max_fault_rate * 100,
               analyses[i].std_deviation,
               analyses[i].avg_execution_time);
    }
    
    // 找出最优算法
    int best_idx = 0;
    for (int i = 1; i < count; i++) {
        if (analyses[i].avg_fault_rate < analyses[best_idx].avg_fault_rate) {
            best_idx = i;
        }
    }
    
    printf("\n最优算法: %s (平均缺页率: %.2f%%)\n",
           get_algorithm_name(analyses[best_idx].algorithm),
           analyses[best_idx].avg_fault_rate * 100);
}

// 统计分析
void statistical_analysis(
    AlgorithmResult* results,
    int count,
    const char* output_file
) {
    if (results == NULL || count == 0) return;
    
    FILE* file = NULL;
    if (output_file != NULL) {
        file = fopen(output_file, "w");
        if (file == NULL) {
            printf("无法创建统计文件: %s\n", output_file);
            file = NULL;
        }
    }
    
    // 计算统计信息
    double sum_fault_rate = 0.0;
    double sum_hit_rate = 0.0;
    double min_fault_rate = results[0].stats.fault_rate;
    double max_fault_rate = results[0].stats.fault_rate;
    
    for (int i = 0; i < count; i++) {
        sum_fault_rate += results[i].stats.fault_rate;
        sum_hit_rate += results[i].stats.hit_rate;
        if (results[i].stats.fault_rate < min_fault_rate) {
            min_fault_rate = results[i].stats.fault_rate;
        }
        if (results[i].stats.fault_rate > max_fault_rate) {
            max_fault_rate = results[i].stats.fault_rate;
        }
    }
    
    double avg_fault_rate = sum_fault_rate / count;
    double avg_hit_rate = sum_hit_rate / count;
    
    // 计算方差和标准差
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = results[i].stats.fault_rate - avg_fault_rate;
        variance += diff * diff;
    }
    double std_deviation = sqrt(variance / count);
    
    // 输出统计信息
    if (file != NULL) {
        fprintf(file, "算法性能统计分析\n");
        fprintf(file, "================\n\n");
        fprintf(file, "测试算法数量: %d\n", count);
        fprintf(file, "\n总体统计:\n");
        fprintf(file, "  平均缺页率: %.2f%%\n", avg_fault_rate * 100);
        fprintf(file, "  平均命中率: %.2f%%\n", avg_hit_rate * 100);
        fprintf(file, "  最小缺页率: %.2f%%\n", min_fault_rate * 100);
        fprintf(file, "  最大缺页率: %.2f%%\n", max_fault_rate * 100);
        fprintf(file, "  标准差: %.4f\n", std_deviation);
        fprintf(file, "\n各算法详细数据:\n");
        for (int i = 0; i < count; i++) {
            fprintf(file, "  %s: 缺页率=%.2f%%, 命中率=%.2f%%\n",
                   get_algorithm_name(results[i].algorithm),
                   results[i].stats.fault_rate * 100,
                   results[i].stats.hit_rate * 100);
        }
        fclose(file);
        printf("统计分析结果已保存到: %s\n", output_file);
    } else {
        printf("\n算法性能统计分析\n");
        printf("================\n\n");
        printf("测试算法数量: %d\n", count);
        printf("\n总体统计:\n");
        printf("  平均缺页率: %.2f%%\n", avg_fault_rate * 100);
        printf("  平均命中率: %.2f%%\n", avg_hit_rate * 100);
        printf("  最小缺页率: %.2f%%\n", min_fault_rate * 100);
        printf("  最大缺页率: %.2f%%\n", max_fault_rate * 100);
        printf("  标准差: %.4f\n", std_deviation);
    }
}

// 生成性能图表数据
void generate_chart_data(
    AlgorithmResult* results,
    int count,
    const char* output_file
) {
    if (results == NULL || count == 0 || output_file == NULL) return;
    
    FILE* file = fopen(output_file, "w");
    if (file == NULL) {
        printf("无法创建图表数据文件: %s\n", output_file);
        return;
    }
    
    // 生成CSV格式数据
    fprintf(file, "算法,缺页次数,命中次数,命中率,缺页率\n");
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s,%d,%d,%.2f,%.2f\n",
               get_algorithm_name(results[i].algorithm),
               results[i].stats.page_faults,
               results[i].stats.page_hits,
               results[i].stats.hit_rate * 100,
               results[i].stats.fault_rate * 100);
    }
    
    fclose(file);
    printf("图表数据已保存到: %s (CSV格式)\n", output_file);
}

