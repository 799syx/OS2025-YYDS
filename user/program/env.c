#include "kernel/include/types.h"
#include "user/user.h"

char buf[128];

// 常见环境变量名列表
char *common_vars[] = {
  "PATH", "HOME", "USER", "SHELL", "TERM", "PWD", "LANG",
  "EDITOR", "HOSTNAME", "PS1", "LOGNAME", "MAIL", 0
};

int main(int argc, char *argv[])
{
  printf("Environment variables:\n");
  
  for (int i = 0; common_vars[i]; i++) {
    if (getenv(common_vars[i], buf, 128) == 0) {
      printf("%s=%s\n", common_vars[i], buf);
    }
  }
  
  exit(0);
}
