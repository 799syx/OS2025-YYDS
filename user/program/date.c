#include "kernel/include/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  int ticks = gettime();
  
  // 假设每秒100个ticks，计算运行时间
  int seconds = ticks / 100;
  int minutes = seconds / 60;
  int hours = minutes / 60;
  int days = hours / 24;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  printf("System uptime: %d days, %d:%02d:%02d\n", days, hours, minutes, seconds);
  printf("Total ticks: %d\n", ticks);
  
  exit(0);
}
