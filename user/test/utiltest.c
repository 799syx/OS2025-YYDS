#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

// ============ factor: 分解质因数 ============
void factor(int n)
{
  if (n <= 1) {
    printf("%d: (无质因数)\n", n);
    return;
  }
  
  printf("%d:", n);
  
  // 分解质因数
  for (int i = 2; i * i <= n; i++) {
    while (n % i == 0) {
      printf(" %d", i);
      n /= i;
    }
  }
  if (n > 1)
    printf(" %d", n);
  
  printf("\n");
}

void test_factor(void)
{
  printf("\n=== factor: 分解质因数 ===\n");
  
  int nums[] = {12, 100, 97, 1024, 360, 13};
  for (int i = 0; i < 6; i++) {
    factor(nums[i]);
  }
  
  printf("factor 测试通过!\n");
}

// ============ seq: 打印数字序列 ============
void seq(int start, int end, int step)
{
  if (step == 0) step = 1;
  
  if (step > 0) {
    for (int i = start; i <= end; i += step)
      printf("%d\n", i);
  } else {
    for (int i = start; i >= end; i += step)
      printf("%d\n", i);
  }
}

void test_seq(void)
{
  printf("\n=== seq: 打印数字序列 ===\n");
  
  printf("seq 1 5:\n");
  seq(1, 5, 1);
  
  printf("seq 2 10 2:\n");
  seq(2, 10, 2);
  
  printf("seq 5 1 -1:\n");
  seq(5, 1, -1);
  
  printf("seq 测试通过!\n");
}

// ============ yes: 重复输出字符串 ============
void yes(char *str, int count)
{
  for (int i = 0; i < count; i++) {
    printf("%s\n", str);
  }
}

void test_yes(void)
{
  printf("\n=== yes: 重复输出字符串 ===\n");
  
  printf("yes \"hello\" (5次):\n");
  yes("hello", 5);
  
  printf("yes \"y\" (3次):\n");
  yes("y", 3);
  
  printf("yes 测试通过!\n");
}

// ============ cal: 显示日历 ============
// 判断是否闰年
int is_leap_year(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 获取某月天数
int days_in_month(int year, int month)
{
  int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year))
    return 29;
  return days[month - 1];
}

// 计算某年某月1日是星期几 (0=周日)
// 使用蔡勒公式简化版
int day_of_week(int year, int month, int day)
{
  if (month < 3) {
    month += 12;
    year--;
  }
  int q = day;
  int m = month;
  int k = year % 100;
  int j = year / 100;
  int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  return ((h + 6) % 7);  // 转换为0=周日
}

void cal(int year, int month)
{
  char *months[] = {"", "January", "February", "March", "April", 
                    "May", "June", "July", "August", 
                    "September", "October", "November", "December"};
  
  printf("    %s %d\n", months[month], year);
  printf("Su Mo Tu We Th Fr Sa\n");
  
  int first_day = day_of_week(year, month, 1);
  int days = days_in_month(year, month);
  
  // 打印开头空格
  for (int i = 0; i < first_day; i++)
    printf("   ");
  
  // 打印日期
  for (int d = 1; d <= days; d++) {
    if (d < 10)
      printf(" %d ", d);
    else
      printf("%d ", d);
    if ((first_day + d) % 7 == 0)
      printf("\n");
  }
  if ((first_day + days) % 7 != 0)
    printf("\n");
}

void test_cal(void)
{
  printf("\n=== cal: 显示日历 ===\n");
  
  // 显示2026年1月日历
  cal(2026, 1);
  printf("\n");
  
  // 显示2024年2月日历（闰年）
  cal(2024, 2);
  
  printf("cal 测试通过!\n");
}

// ============ banner: ASCII 艺术字 ============
// 简单的 5x5 字母表
char *font[26][5] = {
  // A
  {" ### ", "#   #", "#####", "#   #", "#   #"},
  // B
  {"#### ", "#   #", "#### ", "#   #", "#### "},
  // C
  {" ####", "#    ", "#    ", "#    ", " ####"},
  // D
  {"#### ", "#   #", "#   #", "#   #", "#### "},
  // E
  {"#####", "#    ", "#### ", "#    ", "#####"},
  // F
  {"#####", "#    ", "#### ", "#    ", "#    "},
  // G
  {" ####", "#    ", "# ###", "#   #", " ### "},
  // H
  {"#   #", "#   #", "#####", "#   #", "#   #"},
  // I
  {"#####", "  #  ", "  #  ", "  #  ", "#####"},
  // J
  {"#####", "   # ", "   # ", "#  # ", " ##  "},
  // K
  {"#   #", "#  # ", "###  ", "#  # ", "#   #"},
  // L
  {"#    ", "#    ", "#    ", "#    ", "#####"},
  // M
  {"#   #", "## ##", "# # #", "#   #", "#   #"},
  // N
  {"#   #", "##  #", "# # #", "#  ##", "#   #"},
  // O
  {" ### ", "#   #", "#   #", "#   #", " ### "},
  // P
  {"#### ", "#   #", "#### ", "#    ", "#    "},
  // Q
  {" ### ", "#   #", "# # #", "#  # ", " ## #"},
  // R
  {"#### ", "#   #", "#### ", "#  # ", "#   #"},
  // S
  {" ####", "#    ", " ### ", "    #", "#### "},
  // T
  {"#####", "  #  ", "  #  ", "  #  ", "  #  "},
  // U
  {"#   #", "#   #", "#   #", "#   #", " ### "},
  // V
  {"#   #", "#   #", "#   #", " # # ", "  #  "},
  // W
  {"#   #", "#   #", "# # #", "## ##", "#   #"},
  // X
  {"#   #", " # # ", "  #  ", " # # ", "#   #"},
  // Y
  {"#   #", " # # ", "  #  ", "  #  ", "  #  "},
  // Z
  {"#####", "   # ", "  #  ", " #   ", "#####"},
};

void banner(char *text)
{
  // 打印5行
  for (int row = 0; row < 5; row++) {
    for (int i = 0; text[i]; i++) {
      char c = text[i];
      if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';  // 转大写
      
      if (c >= 'A' && c <= 'Z') {
        printf("%s ", font[c - 'A'][row]);
      } else if (c == ' ') {
        printf("      ");  // 空格
      } else {
        printf("      ");  // 其他字符显示空格
      }
    }
    printf("\n");
  }
}

void test_banner(void)
{
  printf("\n=== banner: ASCII 艺术字 ===\n\n");
  
  banner("HI");
  printf("\n");
  
  banner("OS");
  printf("\n");
  
  banner("YYDS");
  
  printf("\nbanner 测试通过!\n");
}

// ============ 主函数 ============
int main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("       实用小工具测试程序\n");
  printf("========================================\n");
  
  test_factor();
  test_seq();
  test_yes();
  test_cal();
  test_banner();
  
  printf("\n========================================\n");
  printf("       所有测试完成!\n");
  printf("========================================\n");
  
  exit(0);
}
