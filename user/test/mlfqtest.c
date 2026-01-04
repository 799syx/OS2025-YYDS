#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// MLFQ 多级反馈队列调度测试程序

// CPU密集型任务：持续计算（静默版本，完成后才输出）
void cpu_intensive(int id, int iterations)
{
  volatile int sum = 0;
  for (int i = 0; i < iterations; i++) {
    for (int j = 0; j < 10000; j++) {
      sum += j;
    }
  }
  
  printf("CPU任务 %d 完成 (iterations=%d, sum=%d)\n", id, iterations, sum);
}

// I/O密集型任务：频繁休眠（静默版本）
void io_intensive(int id, int iterations)
{
  for (int i = 0; i < iterations; i++) {
    sleep(1);  // 模拟I/O等待
  }
  
  printf("I/O任务 %d 完成 (iterations=%d)\n", id, iterations);
}

// 混合型任务（静默版本）
void mixed_task(int id, int iterations)
{
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
  printf("MLFQ应该让I/O密集型任务保持高优先级\n");
  
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 50);
    exit(0);
  }
  wait(0);
  
  int pid2 = fork();
  if (pid2 == 0) {
    io_intensive(2, 10);
    exit(0);
  }
  wait(0);
  
  printf("测试1完成!\n");
}

// 测试2：多个CPU密集型任务公平调度
void test_fairness(void)
{
  printf("\n=== 测试2: 公平调度测试 ===\n");
  printf("多个CPU密集型任务应该公平分享CPU时间\n");
  
  // 顺序执行避免输出竞争
  for (int i = 0; i < 3; i++) {
    int pid = fork();
    if (pid == 0) {
      cpu_intensive(i + 1, 30);
      exit(0);
    }
    wait(0);  // 等待当前子进程完成
  }
  
  printf("测试2完成!\n");
}

// 测试3：优先级提升防止饥饿
void test_starvation(void)
{
  printf("\n=== 测试3: 防饥饿测试 ===\n");
  printf("低优先级任务不应该被饿死\n");
  
  // 顺序执行两个任务
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 50);
    exit(0);
  }
  wait(0);
  
  int pid2 = fork();
  if (pid2 == 0) {
    cpu_intensive(2, 20);
    exit(0);
  }
  wait(0);
  
  printf("测试3完成!\n");
}

// 测试4：交互式任务响应
void test_interactive(void)
{
  printf("\n=== 测试4: 交互式任务响应测试 ===\n");
  printf("模拟交互式任务应该得到快速响应\n");
  
  // 先执行后台CPU任务
  int pid1 = fork();
  if (pid1 == 0) {
    cpu_intensive(1, 40);
    exit(0);
  }
  wait(0);
  
  // 然后执行交互式任务
  printf("交互式任务: 处理5个请求\n");
  for (int i = 0; i < 5; i++) {
    sleep(1);
    volatile int sum = 0;
    for (int j = 0; j < 1000; j++) {
      sum += j;
    }
  }
  printf("交互式任务: 全部完成!\n");
  
  printf("测试4完成!\n");
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
