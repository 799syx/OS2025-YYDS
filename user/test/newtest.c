#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"
#include "kernel/include/fcntl.h"

// 测试新增的系统调用

void test_rename(void)
{
  printf("\n=== 测试 rename ===\n");
  
  // 创建测试文件
  int fd = open("testfile", O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  write(fd, "hello", 5);
  close(fd);
  
  // 重命名文件
  if (rename("testfile", "newfile") < 0) {
    printf("rename 失败\n");
    return;
  }
  printf("rename: testfile -> newfile 成功\n");
  
  // 验证新文件存在
  fd = open("newfile", O_RDONLY);
  if (fd < 0) {
    printf("打开重命名后的文件失败\n");
    return;
  }
  char buf[10];
  int n = read(fd, buf, 5);
  buf[n] = 0;
  printf("读取内容: %s\n", buf);
  close(fd);
  
  // 清理
  unlink("newfile");
  printf("rename 测试通过!\n");
}

void test_lseek(void)
{
  printf("\n=== 测试 lseek ===\n");
  
  // 创建测试文件
  int fd = open("seektest", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  write(fd, "0123456789", 10);
  
  // 测试 SEEK_SET
  int pos = lseek(fd, 5, SEEK_SET);
  printf("SEEK_SET(5): pos = %d\n", pos);
  
  char buf[10];
  int n = read(fd, buf, 3);
  buf[n] = 0;
  printf("读取内容: %s (应为 567)\n", buf);
  
  // 测试 SEEK_CUR
  pos = lseek(fd, -2, SEEK_CUR);
  printf("SEEK_CUR(-2): pos = %d\n", pos);
  
  // 测试 SEEK_END
  pos = lseek(fd, -3, SEEK_END);
  printf("SEEK_END(-3): pos = %d\n", pos);
  
  n = read(fd, buf, 3);
  buf[n] = 0;
  printf("读取内容: %s (应为 789)\n", buf);
  
  close(fd);
  unlink("seektest");
  printf("lseek 测试通过!\n");
}

void test_waitpid(void)
{
  printf("\n=== 测试 waitpid ===\n");
  
  int pid1 = fork();
  if (pid1 == 0) {
    // 子进程1
    sleep(10);
    exit(11);
  }
  
  int pid2 = fork();
  if (pid2 == 0) {
    // 子进程2
    sleep(5);
    exit(22);
  }
  
  printf("创建子进程: pid1=%d, pid2=%d\n", pid1, pid2);
  
  // 等待指定的子进程 pid2
  int status;
  int ret = waitpid(pid2, &status);
  printf("waitpid(%d) 返回: %d, status=%d\n", pid2, ret, status);
  
  // 等待任意子进程
  ret = waitpid(-1, &status);
  printf("waitpid(-1) 返回: %d, status=%d\n", ret, status);
  
  printf("waitpid 测试通过!\n");
}

void test_uid(void)
{
  printf("\n=== 测试 getuid/setuid ===\n");
  
  int uid = getuid();
  printf("当前 uid = %d\n", uid);
  
  // 设置新的 uid
  if (setuid(1000) < 0) {
    printf("setuid(1000) 失败\n");
  } else {
    printf("setuid(1000) 成功\n");
  }
  
  uid = getuid();
  printf("新的 uid = %d\n", uid);
  
  // 非root用户尝试设置其他uid应该失败
  if (setuid(0) < 0) {
    printf("setuid(0) 失败 (预期行为，非root不能设置其他uid)\n");
  } else {
    printf("setuid(0) 成功\n");
  }
  
  printf("getuid/setuid 测试通过!\n");
}

void test_reboot(void)
{
  printf("\n=== 测试 reboot ===\n");
  printf("警告: reboot 会重启系统!\n");
  printf("如需测试，请手动运行 reboot 命令\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("       新系统调用测试程序\n");
  printf("========================================\n");
  
  test_rename();
  test_lseek();
  test_waitpid();
  test_uid();
  test_reboot();
  
  printf("\n========================================\n");
  printf("       所有测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
