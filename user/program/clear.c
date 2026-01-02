#include "kernel/include/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  // ANSI escape sequence to clear screen and move cursor to home
  printf("\033[2J\033[H");
  exit(0);
}
