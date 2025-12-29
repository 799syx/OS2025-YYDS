# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 

# 目录设置
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# 源文件
SRC_FILES = $(SRC_DIR)/page_replacement.c $(SRC_DIR)/utils.c $(SRC_DIR)/analyzer.c $(SRC_DIR)/logger.c $(SRC_DIR)/sequence_generator.c $(SRC_DIR)/performance_monitor.c $(SRC_DIR)/statistics.c
TEST_SRC = $(SRC_DIR)/test.c
BENCHMARK_SRC = $(TEST_DIR)/benchmark.c
COMPREHENSIVE_SRC = $(SRC_DIR)/comprehensive_test.c

# 目标文件
TEST_TARGET = test
BENCHMARK_TARGET = benchmark
DEMO_TARGET = demo
COMPREHENSIVE_TARGET = comprehensive

# 默认目标
all: $(TEST_TARGET) $(BENCHMARK_TARGET) $(DEMO_TARGET) $(COMPREHENSIVE_TARGET)

# 编译测试程序
$(TEST_TARGET): $(TEST_SRC) $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_SRC) $(SRC_FILES) $(LDFLAGS)

# 编译基准测试程序
$(BENCHMARK_TARGET): $(BENCHMARK_SRC) $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(BENCHMARK_TARGET) $(BENCHMARK_SRC) $(SRC_FILES) $(LDFLAGS)

# 编译演示程序
DEMO_SRC = $(SRC_DIR)/demo.c
$(DEMO_TARGET): $(DEMO_SRC) $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(DEMO_TARGET) $(DEMO_SRC) $(SRC_FILES) $(LDFLAGS)

# 编译综合测试程序
$(COMPREHENSIVE_TARGET): $(COMPREHENSIVE_SRC) $(SRC_FILES)
	$(CC) $(CFLAGS) -o $(COMPREHENSIVE_TARGET) $(COMPREHENSIVE_SRC) $(SRC_FILES) $(LDFLAGS) -lm

# 清理
clean:
	rm -f $(TEST_TARGET) $(BENCHMARK_TARGET) $(DEMO_TARGET) $(COMPREHENSIVE_TARGET)
	rm -rf $(BUILD_DIR)
	rm -f execution_report.txt *.csv *.log

# 运行测试
run-test: $(TEST_TARGET)
	./$(TEST_TARGET)

# 运行基准测试
run-benchmark: $(BENCHMARK_TARGET)
	./$(BENCHMARK_TARGET)

# 运行演示程序
run-demo: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# 运行综合测试程序
run-comprehensive: $(COMPREHENSIVE_TARGET)
	./$(COMPREHENSIVE_TARGET)

# 帮助信息
help:
	@echo "可用目标:"
	@echo "  all          - 编译所有程序"
	@echo "  test         - 编译测试程序"
	@echo "  benchmark    - 编译基准测试程序"
	@echo "  demo         - 编译演示程序"
	@echo "  run-test     - 运行测试程序"
	@echo "  run-benchmark - 运行基准测试程序"
	@echo "  run-demo     - 运行演示程序"
	@echo "  comprehensive - 编译综合测试程序"
	@echo "  run-comprehensive - 运行综合测试程序"
	@echo "  clean        - 清理生成的文件"
	@echo "  help         - 显示此帮助信息"

.PHONY: all clean run-test run-benchmark run-demo run-comprehensive help

