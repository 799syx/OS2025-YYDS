/**
 * @file logger.c
 * @brief 日志系统实现
 */

#include "logger.h"
#include "page_replacement.h"
#include <stdarg.h>
#include <string.h>

// 全局日志器
Logger g_logger = {
    .level = LOG_INFO,
    .enable_file = false,
    .enable_console = true,
    .log_file = NULL,
    .file_handle = NULL
};

// 获取日志级别字符串
static const char* get_log_level_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// 初始化日志系统
void logger_init(LogLevel level, bool enable_file, bool enable_console, const char* log_file) {
    g_logger.level = level;
    g_logger.enable_console = enable_console;
    g_logger.enable_file = enable_file;
    
    if (enable_file && log_file != NULL) {
        g_logger.log_file = (char*)malloc(strlen(log_file) + 1);
        strcpy(g_logger.log_file, log_file);
        g_logger.file_handle = fopen(log_file, "w");
        if (g_logger.file_handle == NULL) {
            printf("警告: 无法打开日志文件 %s\n", log_file);
            g_logger.enable_file = false;
        }
    }
}

// 关闭日志系统
void logger_close(void) {
    if (g_logger.file_handle != NULL) {
        fclose(g_logger.file_handle);
        g_logger.file_handle = NULL;
    }
    if (g_logger.log_file != NULL) {
        free(g_logger.log_file);
        g_logger.log_file = NULL;
    }
}

// 记录日志
void logger_log(LogLevel level, const char* format, ...) {
    if (level < g_logger.level) {
        return;
    }
    
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    char log_line[2048];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] %s\n",
             time_str, get_log_level_string(level), buffer);
    
    if (g_logger.enable_console) {
        printf("%s", log_line);
    }
    
    if (g_logger.enable_file && g_logger.file_handle != NULL) {
        fprintf(g_logger.file_handle, "%s", log_line);
        fflush(g_logger.file_handle);
    }
}

// 算法执行开始日志
void log_algorithm_start(AlgorithmType algorithm, int* sequence, int len, int frames) {
    LOG_INFO("算法执行开始: %s", get_algorithm_name(algorithm));
    LOG_INFO("  序列长度: %d", len);
    LOG_INFO("  物理帧数: %d", frames);
    LOG_DEBUG("  页面序列: ");
    for (int i = 0; i < len && i < 20; i++) {
        LOG_DEBUG("%d ", sequence[i]);
    }
    if (len > 20) {
        LOG_DEBUG("...");
    }
}

// 算法执行结束日志
void log_algorithm_end(AlgorithmType algorithm, AlgorithmStats* stats) {
    LOG_INFO("算法执行结束: %s", get_algorithm_name(algorithm));
    LOG_INFO("  总访问次数: %d", stats->total_accesses);
    LOG_INFO("  页面命中次数: %d", stats->page_hits);
    LOG_INFO("  缺页次数: %d", stats->page_faults);
    LOG_INFO("  命中率: %.2f%%", stats->hit_rate * 100);
    LOG_INFO("  缺页率: %.2f%%", stats->fault_rate * 100);
}

// 页面访问日志
void log_page_access(int step, int page_num, bool is_hit, int frame_idx) {
    if (is_hit) {
        LOG_DEBUG("步骤 %d: 访问页面 %d, 命中, 帧索引: %d", step, page_num, frame_idx);
    } else {
        LOG_DEBUG("步骤 %d: 访问页面 %d, 缺失", step, page_num);
    }
}

// 页面缺页日志
void log_page_fault(int step, int page_num, int replaced_page, int frame_idx) {
    if (replaced_page != -1) {
        LOG_INFO("步骤 %d: 缺页, 访问页面 %d, 替换页面 %d, 帧索引: %d",
                 step, page_num, replaced_page, frame_idx);
    } else {
        LOG_INFO("步骤 %d: 缺页, 访问页面 %d, 使用空闲帧, 帧索引: %d",
                 step, page_num, frame_idx);
    }
}

