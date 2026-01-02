#include "kernel/include/types.h"
#include "user/user.h"

char buf[512];

int main(int argc, char *argv[])
{
  int len = procinfo(-1, buf, 512);
  if (len > 0) {
    printf("%s", buf);
  } else {
    printf("ps: failed to get process info\n");
  }
  exit(0);
}
