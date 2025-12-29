/**
 * @file logger.h
 * @brief 日志系统头文件
 * @details 提供日志记录功能，用于调试和性能分析
 * @author 操作系统课程设计
 * @date 2024
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdbool.h>
#include <time.h>

// 日志级别
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// 日志配置
typedef struct {
    LogLevel level;             // 日志级别
    bool enable_file;           // 是否写入文件
    bool enable_console;        // 是否输出到控制台
    char* log_file;             // 日志文件路径
    FILE* file_handle;          // 文件句柄
} Logger;

// 全局日志器
extern Logger g_logger;

// 函数声明
// 初始化日志系统
void logger_init(LogLevel level, bool enable_file, bool enable_console, const char* log_file);

// 关闭日志系统
void logger_close(void);

// 记录日志
void logger_log(LogLevel level, const char* format, ...);

// 便捷宏
#define LOG_DEBUG(...) logger_log(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) logger_log(LOG_INFO, __VA_ARGS__)
#define LOG_WARNING(...) logger_log(LOG_WARNING, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_ERROR, __VA_ARGS__)

// 算法执行日志
void log_algorithm_start(AlgorithmType algorithm, int* sequence, int len, int frames);
void log_algorithm_end(AlgorithmType algorithm, AlgorithmStats* stats);
void log_page_access(int step, int page_num, bool is_hit, int frame_idx);
void log_page_fault(int step, int page_num, int replaced_page, int frame_idx);

#endif // LOGGER_H

