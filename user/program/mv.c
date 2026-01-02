#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  if (argc != 3) {
    printf("Usage: mv source dest\n");
    exit(1);
  }

  if (rename(argv[1], argv[2]) < 0) {
    printf("mv: cannot rename %s to %s\n", argv[1], argv[2]);
    exit(1);
  }

  exit(0);
}
