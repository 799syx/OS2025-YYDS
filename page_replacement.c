/**
 * @file page_replacement.c
 * @brief 页面置换算法核心实现文件
 * @details 实现了7种页面置换算法：FIFO、LRU、OPT、Clock、LFU、改进Clock、自适应算法
 * @author 操作系统课程设计
 * @date 2024
 */

#include "page_replacement.h"

/**
 * @brief 初始化页面帧数组
 * @param frames 页面帧数组指针
 * @param frame_count 物理帧数量
 * @details 将所有页面帧初始化为空状态，清空所有标志位
 */
void init_frames(PageFrame* frames, int frame_count) {
    for (int i = 0; i < frame_count; i++) {
        frames[i].page_num = -1;
        frames[i].access_time = -1;
        frames[i].frequency = 0;
        frames[i].reference_bit = false;
        frames[i].modified_bit = false;
    }
}

/**
 * @brief 在物理帧中查找指定页面
 * @param frames 页面帧数组
 * @param frame_count 物理帧数量
 * @param page_num 要查找的页面号
 * @return 找到返回帧索引，否则返回-1
 */
static int find_page_in_frames(PageFrame* frames, int frame_count, int page_num) {
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].page_num == page_num) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 查找空闲的物理帧
 * @param frames 页面帧数组
 * @param frame_count 物理帧数量
 * @return 找到返回帧索引，否则返回-1
 */
static int find_free_frame(PageFrame* frames, int frame_count) {
    for (int i = 0; i < frame_count; i++) {
        if (frames[i].page_num == -1) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief FIFO (First In First Out) 先进先出页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果（包含统计信息）
 * @details 
 * 算法原理：选择最早进入内存的页面进行置换
 * 优点：实现简单，开销小
 * 缺点：可能出现Belady异常（增加帧数反而增加缺页率）
 * 时间复杂度：O(n)，其中n为序列长度
 */
AlgorithmResult fifo_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_FIFO;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    int next_replace = 0; // FIFO替换指针
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            // 页面命中
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                // 有空闲帧
                frames[free_idx].page_num = page_num;
                result.frame_sequence[i] = free_idx;
            } else {
                // 需要替换
                frames[next_replace].page_num = page_num;
                result.frame_sequence[i] = next_replace;
                next_replace = (next_replace + 1) % frame_count;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief LRU (Least Recently Used) 最近最少使用页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：选择最久未使用的页面进行置换，符合程序局部性原理
 * 优点：性能较好，适合大多数场景
 * 缺点：需要维护访问时间，开销较大
 * 时间复杂度：O(n*m)，其中n为序列长度，m为帧数
 */
AlgorithmResult lru_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_LRU;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            // 页面命中，更新访问时间
            frames[frame_idx].access_time = i;
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                frames[free_idx].access_time = i;
                result.frame_sequence[i] = free_idx;
            } else {
                // 找到最久未使用的页面
                int lru_idx = 0;
                int min_time = frames[0].access_time;
                for (int j = 1; j < frame_count; j++) {
                    if (frames[j].access_time < min_time) {
                        min_time = frames[j].access_time;
                        lru_idx = j;
                    }
                }
                frames[lru_idx].page_num = page_num;
                frames[lru_idx].access_time = i;
                result.frame_sequence[i] = lru_idx;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief OPT (Optimal) 最优页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：选择未来最久不会被访问的页面（理论最优算法）
 * 优点：缺页率最低，作为其他算法的性能基准
 * 缺点：需要预知未来访问序列，实际系统中无法实现
 * 时间复杂度：O(n²)，其中n为序列长度
 * 注意：此算法仅用于理论分析和性能对比
 */
AlgorithmResult opt_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_OPT;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                result.frame_sequence[i] = free_idx;
            } else {
                // 找到未来最久不会被访问的页面
                int replace_idx = 0;
                int farthest = -1;
                
                for (int j = 0; j < frame_count; j++) {
                    int next_use = seq_len; // 默认未来不会使用
                    for (int k = i + 1; k < seq_len; k++) {
                        if (page_sequence[k] == frames[j].page_num) {
                            next_use = k;
                            break;
                        }
                    }
                    if (next_use > farthest) {
                        farthest = next_use;
                        replace_idx = j;
                    }
                }
                frames[replace_idx].page_num = page_num;
                result.frame_sequence[i] = replace_idx;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief Clock 时钟页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：使用循环队列和引用位，类似时钟指针扫描
 * 实现：维护时钟指针，扫描时清除引用位，选择第一个引用位为0的页面
 * 优点：实现相对简单，性能接近LRU，开销较小
 * 缺点：可能需要进行多轮扫描
 * 时间复杂度：O(n*m)，最坏情况下需要多轮扫描
 */
AlgorithmResult clock_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_CLOCK;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    int clock_hand = 0; // 时钟指针
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            // 页面命中，设置引用位
            frames[frame_idx].reference_bit = true;
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                frames[free_idx].reference_bit = true;
                result.frame_sequence[i] = free_idx;
            } else {
                // 使用Clock算法查找替换页面
                while (frames[clock_hand].reference_bit) {
                    frames[clock_hand].reference_bit = false;
                    clock_hand = (clock_hand + 1) % frame_count;
                }
                frames[clock_hand].page_num = page_num;
                frames[clock_hand].reference_bit = true;
                result.frame_sequence[i] = clock_hand;
                clock_hand = (clock_hand + 1) % frame_count;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief LFU (Least Frequently Used) 最不经常使用页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：选择访问频率最低的页面进行置换
 * 优点：适合访问模式稳定的场景
 * 缺点：可能长期保留不再使用的页面（频率计数不会减少）
 * 时间复杂度：O(n*m)，其中n为序列长度，m为帧数
 */
AlgorithmResult lfu_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_LFU;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            // 页面命中，增加访问频率
            frames[frame_idx].frequency++;
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                frames[free_idx].frequency = 1;
                result.frame_sequence[i] = free_idx;
            } else {
                // 找到访问频率最低的页面
                int lfu_idx = 0;
                int min_freq = frames[0].frequency;
                for (int j = 1; j < frame_count; j++) {
                    if (frames[j].frequency < min_freq) {
                        min_freq = frames[j].frequency;
                        lfu_idx = j;
                    }
                }
                frames[lfu_idx].page_num = page_num;
                frames[lfu_idx].frequency = 1;
                result.frame_sequence[i] = lfu_idx;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief Clock-Improved 改进的时钟页面置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：在Clock算法基础上考虑修改位，优先替换未修改的页面
 * 实现策略：按(0,0) -> (0,1) -> (1,0) -> (1,1)的顺序查找替换页面
 * 优点：减少写回操作，提高I/O性能
 * 创新点：综合考虑引用位和修改位，更贴近实际系统需求
 * 时间复杂度：O(n*m)，最坏情况下需要多轮扫描
 */
AlgorithmResult clock_improved_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_CLOCK_IMPROVED;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    int clock_hand = 0;
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            frames[frame_idx].reference_bit = true;
            // 随机设置修改位（模拟写操作）
            if (rand() % 3 == 0) {
                frames[frame_idx].modified_bit = true;
            }
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                frames[free_idx].page_num = page_num;
                frames[free_idx].reference_bit = true;
                frames[free_idx].modified_bit = false;
                result.frame_sequence[i] = free_idx;
            } else {
                // 改进的Clock算法：优先替换(0,0)，其次(0,1)，然后(1,0)，最后(1,1)
                int start_hand = clock_hand;
                bool found = false;
                
                // 第一轮：查找(0,0)
                for (int round = 0; round < 2; round++) {
                    for (int j = 0; j < frame_count; j++) {
                        int idx = (clock_hand + j) % frame_count;
                        bool ref = frames[idx].reference_bit;
                        bool mod = frames[idx].modified_bit;
                        
                        if (round == 0 && !ref && !mod) {
                            // 找到(0,0)
                            frames[idx].page_num = page_num;
                            frames[idx].reference_bit = true;
                            frames[idx].modified_bit = false;
                            result.frame_sequence[i] = idx;
                            clock_hand = (idx + 1) % frame_count;
                            found = true;
                            break;
                        } else if (round == 1 && !ref && mod) {
                            // 找到(0,1)
                            frames[idx].page_num = page_num;
                            frames[idx].reference_bit = true;
                            frames[idx].modified_bit = false;
                            result.frame_sequence[i] = idx;
                            clock_hand = (idx + 1) % frame_count;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                    
                    // 清除引用位
                    for (int j = 0; j < frame_count; j++) {
                        frames[(clock_hand + j) % frame_count].reference_bit = false;
                    }
                }
                
                if (!found) {
                    // 如果还没找到，替换当前指针位置的页面
                    frames[clock_hand].page_num = page_num;
                    frames[clock_hand].reference_bit = true;
                    frames[clock_hand].modified_bit = false;
                    result.frame_sequence[i] = clock_hand;
                    clock_hand = (clock_hand + 1) % frame_count;
                }
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}

/**
 * @brief Adaptive 自适应页面置换算法（创新算法）
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：根据页面访问的局部性特征，动态选择最适合的置换算法
 * 局部性计算：locality = 唯一页面数 / 总访问次数
 * 选择策略：
 *   - 局部性好 (locality < 0.3)：使用LRU算法
 *   - 中等局部性 (0.3 <= locality < 0.6)：使用Clock算法
 *   - 局部性差 (locality >= 0.6)：使用LFU算法
 * 创新点：
 *   1. 自适应选择：根据实际访问模式动态调整策略
 *   2. 性能优化：在不同场景下都能获得接近最优的性能
 *   3. 实用性强：适合实际系统中访问模式变化的情况
 * 时间复杂度：O(n*m)，取决于选择的底层算法
 */
AlgorithmResult adaptive_algorithm(int* page_sequence, int seq_len, int frame_count) {
    // 分析访问模式
    int unique_pages = 0;
    int* page_set = (int*)malloc(seq_len * sizeof(int));
    bool* visited = (bool*)calloc(1000, sizeof(bool)); // 假设页面号小于1000
    
    for (int i = 0; i < seq_len; i++) {
        if (!visited[page_sequence[i]]) {
            visited[page_sequence[i]] = true;
            page_set[unique_pages++] = page_sequence[i];
        }
    }
    free(visited);
    
    // 计算局部性：如果唯一页面数接近序列长度，局部性差；否则局部性好
    double locality = (double)unique_pages / seq_len;
    
    AlgorithmResult result;
    
    // 根据局部性选择算法
    if (locality < 0.3) {
        // 局部性好，使用LRU
        result = lru_algorithm(page_sequence, seq_len, frame_count);
        result.algorithm = ALGORITHM_ADAPTIVE;
    } else if (locality < 0.6) {
        // 中等局部性，使用Clock
        result = clock_algorithm(page_sequence, seq_len, frame_count);
        result.algorithm = ALGORITHM_ADAPTIVE;
    } else {
        // 局部性差，使用LFU
        result = lfu_algorithm(page_sequence, seq_len, frame_count);
        result.algorithm = ALGORITHM_ADAPTIVE;
    }
    
    free(page_set);
    return result;
}

// PBA（页面缓冲置换算法）实现
/**
 * @brief PBA (Page Buffer Algorithm) 页面缓冲置换算法
 * @param page_sequence 页面访问序列数组
 * @param seq_len 访问序列长度
 * @param frame_count 物理帧数量
 * @return AlgorithmResult 算法执行结果
 * @details
 * 算法原理：与FIFO类似，但将被置换的页面放入两个链表（空闲链表和已修改链表）
 * 实现要点：
 *   1. 置换策略与FIFO相同
 *   2. 被置换的页面根据是否修改放入不同链表
 *   3. 下次访问时，先在链表中查找，如果找到则直接调入
 * 优点：减少磁盘I/O操作，提高性能
 * 缺点：需要额外的链表管理开销
 * 时间复杂度：O(n*m)，其中n为序列长度，m为帧数
 */
AlgorithmResult pba_algorithm(int* page_sequence, int seq_len, int frame_count) {
    PageFrame* frames = (PageFrame*)malloc(frame_count * sizeof(PageFrame));
    init_frames(frames, frame_count);
    
    // 页面缓冲链表（简化实现：使用数组模拟）
    #define BUFFER_SIZE 100
    int free_buffer[BUFFER_SIZE];      // 空闲链表
    int modified_buffer[BUFFER_SIZE];  // 已修改链表
    int free_count = 0;
    int modified_count = 0;
    
    AlgorithmResult result;
    result.algorithm = ALGORITHM_PBA;
    result.stats.page_faults = 0;
    result.stats.page_hits = 0;
    result.stats.total_accesses = seq_len;
    result.frame_sequence = (int*)malloc(seq_len * sizeof(int));
    result.frame_count = frame_count;
    
    int next_replace = 0; // FIFO替换指针
    
    for (int i = 0; i < seq_len; i++) {
        int page_num = page_sequence[i];
        int frame_idx = find_page_in_frames(frames, frame_count, page_num);
        
        if (frame_idx != -1) {
            // 页面命中
            result.stats.page_hits++;
            result.frame_sequence[i] = frame_idx;
        } else {
            // 页面缺失，先在缓冲链表中查找
            bool found_in_buffer = false;
            int buffer_idx = -1;
            
            // 先在空闲链表中查找
            for (int j = 0; j < free_count; j++) {
                if (free_buffer[j] == page_num) {
                    found_in_buffer = true;
                    buffer_idx = j;
                    // 从链表中移除
                    for (int k = j; k < free_count - 1; k++) {
                        free_buffer[k] = free_buffer[k + 1];
                    }
                    free_count--;
                    break;
                }
            }
            
            // 如果没找到，在已修改链表中查找
            if (!found_in_buffer) {
                for (int j = 0; j < modified_count; j++) {
                    if (modified_buffer[j] == page_num) {
                        found_in_buffer = true;
                        buffer_idx = j;
                        // 从链表中移除
                        for (int k = j; k < modified_count - 1; k++) {
                            modified_buffer[k] = modified_buffer[k + 1];
                        }
                        modified_count--;
                        break;
                    }
                }
            }
            
            result.stats.page_faults++;
            int free_idx = find_free_frame(frames, frame_count);
            
            if (free_idx != -1) {
                // 有空闲帧
                frames[free_idx].page_num = page_num;
                result.frame_sequence[i] = free_idx;
            } else {
                // 需要替换
                int replaced_page = frames[next_replace].page_num;
                bool is_modified = frames[next_replace].modified_bit;
                
                // 将被替换的页面放入相应的缓冲链表
                if (is_modified && modified_count < BUFFER_SIZE) {
                    modified_buffer[modified_count++] = replaced_page;
                } else if (!is_modified && free_count < BUFFER_SIZE) {
                    free_buffer[free_count++] = replaced_page;
                }
                
                frames[next_replace].page_num = page_num;
                frames[next_replace].modified_bit = false;
                result.frame_sequence[i] = next_replace;
                next_replace = (next_replace + 1) % frame_count;
            }
        } else {
            // 页面命中时，随机设置修改位（模拟写操作）
            if (rand() % 3 == 0) {
                frames[frame_idx].modified_bit = true;
            }
        }
    }
    
    result.stats.hit_rate = (double)result.stats.page_hits / seq_len;
    result.stats.fault_rate = (double)result.stats.page_faults / seq_len;
    
    free(frames);
    return result;
}
