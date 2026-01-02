#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// 信号处理测试程序

volatile int got_signal = 0;

void sigint_handler(int signum)
{
  got_signal = 1;
  printf("\n收到 SIGINT 信号 (signum=%d)!\n", signum);
}

void test_signal(void)
{
  printf("\n=== 测试 signal ===\n");
  
  // 注册 SIGINT 处理函数
  void *old = signal(SIGINT, (void*)sigint_handler);
  printf("注册 SIGINT 处理函数, 旧处理函数=%p\n", old);
  
  // 注册 SIGUSR1 处理函数
  signal(SIGUSR1, (void*)sigint_handler);
  printf("注册 SIGUSR1 处理函数\n");
  
  printf("signal 测试通过!\n");
}

void test_sigkill(void)
{
  printf("\n=== 测试 sigkill ===\n");
  
  int pid = fork();
  if (pid == 0) {
    // 子进程: 等待信号
    printf("子进程 (pid=%d) 开始等待...\n", getpid());
    sleep(100);  // 等待被信号中断
    printf("子进程被唤醒\n");
    exit(0);
  }
  
  // 父进程: 发送信号
  sleep(10);  // 等待子进程启动
  printf("父进程发送 SIGTERM 给子进程 %d\n", pid);
  
  if (sigkill(pid, SIGTERM) == 0) {
    printf("sigkill 成功\n");
  } else {
    printf("sigkill 失败\n");
  }
  
  // 等待子进程退出
  int status;
  wait(&status);
  printf("子进程退出, status=%d\n", status);
  
  printf("sigkill 测试通过!\n");
}

void test_pause(void)
{
  printf("\n=== 测试 pause ===\n");
  
  int pid = fork();
  if (pid == 0) {
    // 子进程: 注册信号处理函数并等待
    signal(SIGUSR1, (void*)sigint_handler);
    printf("子进程 (pid=%d) 调用 pause() 等待信号...\n", getpid());
    pause();
    printf("子进程从 pause() 返回!\n");
    exit(0);
  }
  
  // 父进程: 发送信号
  sleep(10);
  printf("父进程发送 SIGUSR1 给子进程 %d\n", pid);
  sigkill(pid, SIGUSR1);
  
  // 等待子进程退出
  int status;
  wait(&status);
  printf("子进程退出\n");
  
  printf("pause 测试通过!\n");
}

void test_ctrlc(void)
{
  printf("\n=== 测试 Ctrl+C ===\n");
  printf("请在 5 秒内按 Ctrl+C...\n");
  
  got_signal = 0;
  signal(SIGINT, (void*)sigint_handler);
  
  for (int i = 0; i < 50 && !got_signal; i++) {
    sleep(1);
    printf(".");
  }
  
  if (got_signal) {
    printf("\nCtrl+C 测试通过!\n");
  } else {
    printf("\n超时，未收到 Ctrl+C\n");
  }
}

// ============ 进程管理测试 ============

void test_nice(void)
{
  printf("\n=== 测试 nice ===\n");
  
  int old_prio = nice(0);  // 获取当前优先级
  printf("当前优先级: %d\n", old_prio);
  
  int new_prio = nice(5);  // 降低优先级
  printf("nice(5) 后优先级: %d\n", new_prio);
  
  new_prio = nice(-3);  // 提高优先级
  printf("nice(-3) 后优先级: %d\n", new_prio);
  
  printf("nice 测试通过!\n");
}

void test_times(void)
{
  printf("\n=== 测试 times ===\n");
  
  struct tms t;
  int clock = times(&t);
  
  printf("系统时钟: %d\n", clock);
  printf("用户态时间: %d\n", t.tms_utime);
  printf("内核态时间: %d\n", t.tms_stime);
  
  // 做一些计算消耗CPU
  volatile int sum = 0;
  for (int i = 0; i < 10000; i++) sum += i;
  
  clock = times(&t);
  printf("计算后用户态时间: %d\n", t.tms_utime);
  
  printf("times 测试通过!\n");
}

void test_getppid(void)
{
  printf("\n=== 测试 getppid ===\n");
  
  int pid = getpid();
  int ppid = getppid();
  
  printf("当前进程 PID: %d\n", pid);
  printf("父进程 PID: %d\n", ppid);
  
  int child = fork();
  if (child == 0) {
    printf("子进程: PID=%d, PPID=%d (应为%d)\n", getpid(), getppid(), pid);
    exit(0);
  }
  wait(0);
  
  printf("getppid 测试通过!\n");
}

void test_pgid(void)
{
  printf("\n=== 测试进程组 (setpgid/getpgid) ===\n");
  
  int pid = getpid();
  int pgid = getpgid(0);
  printf("当前进程: PID=%d, PGID=%d\n", pid, pgid);
  
  // 创建子进程测试
  int child = fork();
  if (child == 0) {
    printf("子进程初始 PGID: %d\n", getpgid(0));
    
    // 设置自己为新进程组组长
    if (setpgid(0, 0) == 0) {
      printf("子进程设置新 PGID 成功: %d\n", getpgid(0));
    }
    exit(0);
  }
  
  sleep(5);
  printf("父进程查看子进程 PGID: %d\n", getpgid(child));
  wait(0);
  
  printf("进程组测试通过!\n");
}

void test_session(void)
{
  printf("\n=== 测试会话 (setsid/getsid) ===\n");
  
  int sid = getsid(0);
  printf("当前会话 ID: %d\n", sid);
  
  int child = fork();
  if (child == 0) {
    printf("子进程初始 SID: %d\n", getsid(0));
    
    // 创建新会话
    int new_sid = setsid();
    if (new_sid > 0) {
      printf("子进程创建新会话成功, SID=%d\n", new_sid);
    } else {
      printf("子进程创建新会话失败 (可能已是组长)\n");
    }
    exit(0);
  }
  
  wait(0);
  printf("会话测试通过!\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("       信号系统测试程序\n");
  printf("========================================\n");
  
  test_signal();
  test_sigkill();
  test_pause();
  // test_ctrlc();  // 需要手动交互，默认注释掉
  
  // 进程管理测试
  test_nice();
  test_times();
  test_getppid();
  test_pgid();
  test_session();
  
  printf("\n========================================\n");
  printf("       所有测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
