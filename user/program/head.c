#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user/user.h"

char buf[512];

int main(int argc, char *argv[])
{
  int fd, n, lines = 10;
  int line_count = 0;
  char *file = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == 'n') {
      if (argv[i][2]) {
        lines = atoi(&argv[i][2]);
      } else if (i + 1 < argc) {
        lines = atoi(argv[++i]);
      }
    } else {
      file = argv[i];
    }
  }

  if (file == 0) {
    fd = 0;  // stdin
  } else {
    fd = open(file, O_RDONLY);
    if (fd < 0) {
      printf("head: cannot open %s\n", file);
      exit(1);
    }
  }

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (int i = 0; i < n && line_count < lines; i++) {
      printf("%c", buf[i]);
      if (buf[i] == '\n') {
        line_count++;
      }
    }
    if (line_count >= lines) break;
  }

  if (fd != 0) close(fd);
  exit(0);
}
