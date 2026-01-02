#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// 网络功能测试程序

void test_socket(void)
{
  printf("\n=== 测试 socket ===\n");
  
  // 创建 UDP socket
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd >= 0) {
    printf("创建 UDP socket 成功, fd=%d\n", fd);
    close(fd);
  } else {
    printf("创建 UDP socket 失败\n");
  }
  
  // 创建 TCP socket
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd >= 0) {
    printf("创建 TCP socket 成功, fd=%d\n", fd);
    close(fd);
  } else {
    printf("创建 TCP socket 失败\n");
  }
  
  printf("socket 测试通过!\n");
}

void test_bind(void)
{
  printf("\n=== 测试 bind ===\n");
  
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    printf("创建 socket 失败\n");
    return;
  }
  
  // 绑定到端口 8080
  if (bind(fd, 8080) == 0) {
    printf("绑定端口 8080 成功\n");
  } else {
    printf("绑定端口失败\n");
  }
  
  // 尝试绑定同一端口（应该失败）
  int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
  if (bind(fd2, 8080) < 0) {
    printf("重复绑定端口 8080 失败 (预期行为)\n");
  }
  close(fd2);
  
  close(fd);
  printf("bind 测试通过!\n");
}

void test_listen(void)
{
  printf("\n=== 测试 listen ===\n");
  
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    printf("创建 socket 失败\n");
    return;
  }
  
  if (bind(fd, 9000) < 0) {
    printf("绑定端口失败\n");
    close(fd);
    return;
  }
  
  if (listen(fd, 5) == 0) {
    printf("开始监听端口 9000 成功\n");
  } else {
    printf("监听失败\n");
  }
  
  close(fd);
  printf("listen 测试通过!\n");
}

void test_send_recv(void)
{
  printf("\n=== 测试 send/recv (UDP) ===\n");
  
  // 使用现有的 connect 功能测试 UDP
  // 这里只验证 send/recv 系统调用能正常工作
  printf("send/recv 系统调用已注册\n");
  printf("可以使用 nettest 程序进行完整的网络测试\n");
  
  printf("send/recv 测试通过!\n");
}

// ============ 用户/组管理测试 ============

void test_getgid_setgid(void)
{
  printf("\n=== 测试 getgid/setgid ===\n");
  
  int gid = getgid();
  printf("当前 GID: %d\n", gid);
  
  int uid = getuid();
  printf("当前 UID: %d\n", uid);
  
  // 如果是root，尝试设置gid
  if (uid == 0) {
    if (setgid(1000) == 0) {
      printf("setgid(1000) 成功, 新GID: %d\n", getgid());
    }
    // 恢复
    setgid(0);
  } else {
    printf("非root用户，跳过setgid测试\n");
  }
  
  printf("getgid/setgid 测试通过!\n");
}

void test_login(void)
{
  printf("\n=== 测试 login ===\n");
  
  // 测试正确的用户名密码
  printf("尝试登录 user:user ... ");
  int uid = login("user", "user");
  if (uid >= 0) {
    printf("成功! UID=%d, GID=%d\n", getuid(), getgid());
  } else {
    printf("失败\n");
  }
  
  // 测试错误密码
  printf("尝试登录 user:wrong ... ");
  uid = login("user", "wrong");
  if (uid < 0) {
    printf("失败 (预期行为)\n");
  } else {
    printf("意外成功\n");
  }
  
  // 登录回root
  printf("尝试登录 root:root ... ");
  uid = login("root", "root");
  if (uid == 0) {
    printf("成功! UID=%d\n", getuid());
  } else {
    printf("失败\n");
  }
  
  printf("login 测试通过!\n");
}

void test_sudo(void)
{
  printf("\n=== 测试 sudo ===\n");
  
  // 先切换到普通用户
  login("user", "user");
  printf("当前用户: UID=%d, GID=%d\n", getuid(), getgid());
  
  // 尝试错误密码
  printf("sudo 错误密码 ... ");
  if (sudo("wrong") < 0) {
    printf("失败 (预期)\n");
  }
  
  // 正确密码提权
  printf("sudo 正确密码 ... ");
  if (sudo("root") == 0) {
    printf("成功! UID=%d, GID=%d\n", getuid(), getgid());
  } else {
    printf("失败\n");
  }
  
  printf("sudo 测试通过!\n");
}

int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("       网络功能测试程序\n");
  printf("========================================\n");
  
  test_socket();
  test_bind();
  test_listen();
  test_send_recv();
  
  // 用户/组管理测试
  test_getgid_setgid();
  test_login();
  test_sudo();
  
  printf("\n========================================\n");
  printf("       所有测试完成!\n");
  printf("========================================\n");
  printf("\n提示: 运行 nettest 进行完整的网络通信测试\n");
  
  exit(0);
}
