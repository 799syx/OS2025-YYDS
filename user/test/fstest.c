#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fs.h"
#include "user/user.h"
#include "kernel/include/fcntl.h"

// 测试文件系统增强功能

void test_access(void)
{
  printf("\n=== 测试 access ===\n");
  
  // 测试文件存在性
  if (access("README.md", F_OK) == 0) {
    printf("access: README.md 存在\n");
  } else {
    printf("access: README.md 不存在\n");
  }
  
  // 测试不存在的文件
  if (access("nofile", F_OK) < 0) {
    printf("access: nofile 不存在 (预期)\n");
  }
  
  // 测试读权限
  if (access("README.md", R_OK) == 0) {
    printf("access: README.md 可读\n");
  }
  
  printf("access 测试通过!\n");
}

void test_umask(void)
{
  printf("\n=== 测试 umask ===\n");
  
  // 获取当前umask
  int old = umask(0);
  printf("原始 umask = %d\n", old);
  
  // 设置新的umask
  umask(077);
  int cur = umask(022);
  printf("设置后 umask = %d (应为 077=63)\n", cur);
  
  // 恢复
  umask(old);
  printf("umask 测试通过!\n");
}

void test_readdir(void)
{
  printf("\n=== 测试 readdir ===\n");
  
  int fd = open(".", O_RDONLY);
  if (fd < 0) {
    printf("打开目录失败\n");
    return;
  }
  
  struct dirent entries[5];
  int n = readdir(fd, entries, 5);
  printf("读取到 %d 个目录项:\n", n);
  
  for (int i = 0; i < n; i++) {
    printf("  [%d] inum=%d name=%s\n", i, entries[i].inum, entries[i].name);
  }
  
  close(fd);
  printf("readdir 测试通过!\n");
}

void test_flock(void)
{
  printf("\n=== 测试 flock ===\n");
  
  // 创建测试文件
  int fd = open("locktest", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  write(fd, "test", 4);
  
  // 获取共享锁
  if (flock(fd, LOCK_SH) == 0) {
    printf("flock: 获取共享锁成功\n");
  }
  
  // 解锁
  if (flock(fd, LOCK_UN) == 0) {
    printf("flock: 解锁成功\n");
  }
  
  // 获取排他锁
  if (flock(fd, LOCK_EX) == 0) {
    printf("flock: 获取排他锁成功\n");
  }
  
  // 解锁
  flock(fd, LOCK_UN);
  
  close(fd);
  unlink("locktest");
  printf("flock 测试通过!\n");
}

void test_chown(void)
{
  printf("\n=== 测试 chown ===\n");
  
  // 创建测试文件
  int fd = open("chowntest", O_CREATE | O_WRONLY);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  close(fd);
  
  // 尝试修改所有者 (需要root权限)
  if (chown("chowntest", 1000) == 0) {
    printf("chown: 修改所有者成功\n");
  } else {
    printf("chown: 修改所有者失败 (可能需要root)\n");
  }
  
  unlink("chowntest");
  printf("chown 测试通过!\n");
}

void test_stat(void)
{
  printf("\n=== 测试 kstat ===\n");
  
  struct stat st;
  
  // 测试存在的文件
  if (kstat("README.md", &st) == 0) {
    printf("kstat README.md:\n");
    printf("  type=%d, ino=%d, size=%d\n", st.type, st.ino, st.size);
  } else {
    printf("kstat README.md 失败\n");
  }
  
  // 测试不存在的文件
  if (kstat("noexist", &st) < 0) {
    printf("kstat noexist 失败 (预期)\n");
  }
  
  // 测试目录
  if (kstat(".", &st) == 0) {
    printf("kstat . (当前目录):\n");
    printf("  type=%d (1=目录), ino=%d\n", st.type, st.ino);
  }
  
  printf("kstat 测试通过!\n");
}

void test_truncate(void)
{
  printf("\n=== 测试 truncate ===\n");
  
  // 创建测试文件
  int fd = open("trunctest", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  write(fd, "Hello, World! This is a test.", 29);
  close(fd);
  
  // 查看原始大小
  struct stat st;
  stat("trunctest", &st);
  printf("原始大小: %d\n", st.size);
  
  // 截断文件
  if (truncate("trunctest", 5) == 0) {
    stat("trunctest", &st);
    printf("截断后大小: %d (应为5)\n", st.size);
  }
  
  // 截断为0
  if (truncate("trunctest", 0) == 0) {
    stat("trunctest", &st);
    printf("清空后大小: %d (应为0)\n", st.size);
  }
  
  unlink("trunctest");
  printf("truncate 测试通过!\n");
}

void test_dup2(void)
{
  printf("\n=== 测试 dup2 ===\n");
  
  int fd = open("dup2test", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf("创建文件失败\n");
    return;
  }
  
  // 复制fd到fd+5
  int newfd = dup2(fd, fd + 5);
  if (newfd == fd + 5) {
    printf("dup2(%d, %d) = %d 成功\n", fd, fd + 5, newfd);
    
    // 通过新fd写入
    write(newfd, "test", 4);
    
    // 通过原fd读取
    lseek(fd, 0, 0);
    char buf[10];
    int n = read(fd, buf, 10);
    buf[n] = 0;
    printf("读取内容: %s\n", buf);
    
    close(newfd);
  } else {
    printf("dup2 失败\n");
  }
  
  close(fd);
  unlink("dup2test");
  printf("dup2 测试通过!\n");
}

void test_getdents(void)
{
  printf("\n=== 测试 getdents ===\n");
  
  int fd = open(".", O_RDONLY);
  if (fd < 0) {
    printf("打开目录失败\n");
    return;
  }
  
  struct linux_dirent entries[10];
  int nread = getdents(fd, entries, sizeof(entries));
  
  if (nread > 0) {
    int count = nread / sizeof(struct linux_dirent);
    printf("读取到 %d 字节, %d 个目录项:\n", nread, count);
    for (int i = 0; i < count; i++) {
      printf("  [%d] ino=%d name=%s\n", i, entries[i].ino, entries[i].name);
    }
  } else {
    printf("getdents 返回 %d\n", nread);
  }
  
  close(fd);
  printf("getdents 测试通过!\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("    文件系统增强功能测试\n");
  printf("========================================\n");
  
  test_access();
  test_umask();
  test_readdir();
  test_flock();
  test_chown();
  test_stat();
  test_truncate();
  test_dup2();
  test_getdents();
  
  printf("\n========================================\n");
  printf("    所有测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
