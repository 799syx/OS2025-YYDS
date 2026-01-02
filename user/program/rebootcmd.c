#include "kernel/include/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  printf("正在重启系统...\n");
  reboot();
  // 不会到达这里
  exit(0);
}
