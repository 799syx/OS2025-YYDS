#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user/user.h"

char buf[512];

int main(int argc, char *argv[])
{
  int fd_src, fd_dst, n;

  if (argc != 3) {
    printf("Usage: cp source dest\n");
    exit(1);
  }

  if ((fd_src = open(argv[1], O_RDONLY)) < 0) {
    printf("cp: cannot open %s\n", argv[1]);
    exit(1);
  }

  if ((fd_dst = open(argv[2], O_CREATE | O_WRONLY)) < 0) {
    printf("cp: cannot create %s\n", argv[2]);
    close(fd_src);
    exit(1);
  }

  while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
    if (write(fd_dst, buf, n) != n) {
      printf("cp: write error\n");
      close(fd_src);
      close(fd_dst);
      exit(1);
    }
  }

  if (n < 0) {
    printf("cp: read error\n");
  }

  close(fd_src);
  close(fd_dst);
  exit(0);
}
