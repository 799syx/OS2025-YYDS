#include "kernel/include/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: export NAME=VALUE\n");
    printf("       export NAME VALUE\n");
    exit(1);
  }

  char *name = argv[1];
  char *value = 0;
  
  // 查找 '=' 分隔符
  for (char *p = argv[1]; *p; p++) {
    if (*p == '=') {
      *p = 0;
      value = p + 1;
      break;
    }
  }
  
  // 如果没有 '='，使用第二个参数
  if (value == 0 && argc >= 3) {
    value = argv[2];
  }
  
  if (value == 0) {
    printf("export: missing value\n");
    exit(1);
  }

  if (setenv(name, value) < 0) {
    printf("export: failed to set %s\n", name);
    exit(1);
  }
  
  printf("%s=%s\n", name, value);
  exit(0);
}
