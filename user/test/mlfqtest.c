#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// MLFQ 多级反馈队列调度测试程序

// CPU密集型任务：持续计算
void cpu_intensive(int id, int iterations)
{
  printf("CPU任务 %d 开始 (iterations=%d)\n", id, iterations);
  
  volatile int sum = 0;
  for (int i = 0; i < iterations; i++) {
    for (int j = 0; j < 10000; j++) {
      sum += j;
    }
  }
  
  printf("CPU任务 %d 完成, sum=%d\n", id, sum);
}

// I/O密集型任务：频繁休眠
void io_intensive(int id, int iterations)
{
  printf("I/O任务 %d 开始 (iterations=%d)\n", id, iterations);
  
  for (int i = 0; i < iterations; i++) {
    sleep(1);  // 模拟I/O等待
    printf("I/O任务 %d: 第 %d 次I/O完成\n", id, i + 1);
  }
  
  printf("I/O任务 %d 完成\n", id);
}

// 混合型任务
void mixed_task(int id, int iterations)
{
  printf("混合任务 %d 开始\n", id);
  
  for (int i = 0; i < iterations; i++) {
    // CPU计算
    volatile int sum = 0;
    for (int j = 0; j < 5000; j++) {
      sum += j;
    }
    // I/O等待
    sleep(1);
  }
  
  printf("混合任务 %d 完成\n", id);
}

// 测试1：CPU密集型 vs I/O密集型
void test_cpu_vs_io(void)
{
  printf("\n=== 测试1: CPU密集型 vs I/O密集型 ===\n");
  printf("MLFQ应该让I/O密集型任务保持高优先级，响应更快\n\n");
  
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 50);
    exit(0);
  }
  
  int pid2 = fork();
  if (pid2 == 0) {
    io_intensive(2, 10);
    exit(0);
  }
  
  wait(0);
  wait(0);
  
  printf("\n测试1完成!\n");
}

// 测试2：多个CPU密集型任务公平调度
void test_fairness(void)
{
  printf("\n=== 测试2: 公平调度测试 ===\n");
  printf("多个CPU密集型任务应该公平分享CPU时间\n\n");
  
  for (int i = 0; i < 3; i++) {
    int pid = fork();
    if (pid == 0) {
      cpu_intensive(i + 1, 30);
      exit(0);
    }
  }
  
  for (int i = 0; i < 3; i++) {
    wait(0);
  }
  
  printf("\n测试2完成!\n");
}

// 测试3：优先级提升防止饥饿
void test_starvation(void)
{
  printf("\n=== 测试3: 防饥饿测试 ===\n");
  printf("低优先级任务不应该被饿死\n\n");
  
  // 启动一个长时间运行的CPU任务
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 100);
    exit(0);
  }
  
  // 等待一段时间后启动另一个任务
  sleep(5);
  
  int pid2 = fork();
  if (pid2 == 0) {
    printf("后启动的任务开始执行\n");
    cpu_intensive(2, 20);
    exit(0);
  }
  
  wait(0);
  wait(0);
  
  printf("\n测试3完成!\n");
}

// 测试4：交互式任务响应
void test_interactive(void)
{
  printf("\n=== 测试4: 交互式任务响应测试 ===\n");
  printf("模拟交互式任务应该得到快速响应\n\n");
  
  // 启动后台CPU任务
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 80);
    exit(0);
  }
  
  // 模拟交互式任务（短暂计算+等待）
  for (int i = 0; i < 5; i++) {
    sleep(2);
    printf("交互式请求 %d: 处理中...\n", i + 1);
    // 短暂计算
    volatile int sum = 0;
    for (int j = 0; j < 1000; j++) {
      sum += j;
    }
    printf("交互式请求 %d: 完成!\n", i + 1);
  }
  
  wait(0);
  
  printf("\n测试4完成!\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("   MLFQ 多级反馈队列调度测试程序\n");
  printf("========================================\n");
  printf("\nMLFQ 特点:\n");
  printf("- 4个优先级队列 (Q0=最高, Q3=最低)\n");
  printf("- 时间片: Q0=2, Q1=4, Q2=8, Q3=16 ticks\n");
  printf("- CPU密集型进程逐渐降级\n");
  printf("- I/O密集型进程保持高优先级\n");
  printf("- 定期优先级提升防止饥饿\n");
  
  test_cpu_vs_io();
  test_fairness();
  test_starvation();
  test_interactive();
  
  printf("\n========================================\n");
  printf("       所有MLFQ测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
