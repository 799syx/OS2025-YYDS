#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "user/user.h"

#define BUFSIZE 4096
char buf[BUFSIZE];
char *lines[256];

int main(int argc, char *argv[])
{
  int fd, n, nlines = 10;
  char *file = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == 'n') {
      if (argv[i][2]) {
        nlines = atoi(&argv[i][2]);
      } else if (i + 1 < argc) {
        nlines = atoi(argv[++i]);
      }
    } else {
      file = argv[i];
    }
  }

  if (file == 0) {
    fd = 0;
  } else {
    fd = open(file, O_RDONLY);
    if (fd < 0) {
      printf("tail: cannot open %s\n", file);
      exit(1);
    }
  }

  int total = 0;
  while ((n = read(fd, buf + total, BUFSIZE - total - 1)) > 0) {
    total += n;
    if (total >= BUFSIZE - 1) break;
  }
  buf[total] = 0;

  int line_count = 0;
  lines[line_count++] = buf;
  for (int i = 0; i < total && line_count < 255; i++) {
    if (buf[i] == '\n' && i + 1 < total) {
      lines[line_count++] = &buf[i + 1];
    }
  }

  int start = line_count > nlines ? line_count - nlines : 0;
  for (int i = start; i < line_count; i++) {
    char *p = lines[i];
    while (*p && *p != '\n') printf("%c", *p++);
    if (*p == '\n') printf("\n");
  }

  if (fd != 0) close(fd);
  exit(0);
}
