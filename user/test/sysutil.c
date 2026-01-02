#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// 系统工具功能测试

void test_gettime(void)
{
  printf("\n=== 测试 gettime ===\n");
  
  int t1 = gettime();
  printf("当前系统时间 (ticks): %d\n", t1);
  
  sleep(10);  // 等待一些ticks
  
  int t2 = gettime();
  printf("等待后系统时间: %d (增加了 %d ticks)\n", t2, t2 - t1);
  
  printf("gettime 测试通过!\n");
}

void test_hostname(void)
{
  printf("\n=== 测试 hostname ===\n");
  
  char buf[64];
  
  // 获取主机名
  if (gethostname(buf, 64) == 0) {
    printf("当前主机名: %s\n", buf);
  } else {
    printf("获取主机名失败\n");
  }
  
  // 设置新主机名
  if (sethostname("my-bird-os", 10) == 0) {
    printf("设置主机名成功\n");
  }
  
  // 再次获取
  if (gethostname(buf, 64) == 0) {
    printf("新主机名: %s\n", buf);
  }
  
  // 恢复
  sethostname("bird-os", 7);
  
  printf("hostname 测试通过!\n");
}

void test_env(void)
{
  printf("\n=== 测试环境变量 ===\n");
  
  char buf[128];
  
  // 设置环境变量
  if (setenv("PATH", "/bin:/usr/bin") == 0) {
    printf("设置 PATH 成功\n");
  }
  
  if (setenv("HOME", "/home/user") == 0) {
    printf("设置 HOME 成功\n");
  }
  
  if (setenv("USER", "root") == 0) {
    printf("设置 USER 成功\n");
  }
  
  // 获取环境变量
  if (getenv("PATH", buf, 128) == 0) {
    printf("PATH = %s\n", buf);
  }
  
  if (getenv("HOME", buf, 128) == 0) {
    printf("HOME = %s\n", buf);
  }
  
  if (getenv("USER", buf, 128) == 0) {
    printf("USER = %s\n", buf);
  }
  
  // 获取不存在的变量
  if (getenv("NOEXIST", buf, 128) < 0) {
    printf("NOEXIST 不存在 (预期)\n");
  }
  
  printf("环境变量测试通过!\n");
}

void test_procinfo(void)
{
  printf("\n=== 测试 /proc (procinfo) ===\n");
  
  char buf[512];
  
  // 列出所有进程
  printf("--- 所有进程 ---\n");
  int len = procinfo(-1, buf, 512);
  if (len > 0) {
    printf("%s", buf);
  }
  
  // 获取当前进程信息
  int pid = getpid();
  printf("\n--- 进程 %d 信息 ---\n", pid);
  len = procinfo(pid, buf, 512);
  if (len > 0) {
    printf("%s", buf);
  }
  
  printf("procinfo 测试通过!\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("       系统工具功能测试\n");
  printf("========================================\n");
  
  test_gettime();
  test_hostname();
  test_env();
  test_procinfo();
  
  printf("\n========================================\n");
  printf("       所有测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
