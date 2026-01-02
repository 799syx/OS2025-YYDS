#include "kernel/include/types.h"
#include "user/user.h"

char buf[64];

int main(int argc, char *argv[])
{
  if (argc == 1) {
    // 显示主机名
    if (gethostname(buf, 64) == 0) {
      printf("%s\n", buf);
    } else {
      printf("hostname: failed to get hostname\n");
      exit(1);
    }
  } else {
    // 设置主机名
    int len = strlen(argv[1]);
    if (sethostname(argv[1], len) < 0) {
      printf("hostname: failed to set hostname\n");
      exit(1);
    }
    printf("Hostname set to: %s\n", argv[1]);
  }
  
  exit(0);
}
