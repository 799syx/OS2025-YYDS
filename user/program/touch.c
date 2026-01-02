#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int fd;

  if (argc < 2) {
    printf("Usage: touch file...\n");
    exit(1);
  }

  for (int i = 1; i < argc; i++) {
    fd = open(argv[i], O_CREATE | O_RDWR);
    if (fd < 0) {
      printf("touch: cannot touch %s\n", argv[i]);
    } else {
      close(fd);
    }
  }

  exit(0);
}
