//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c, locking;
  char *s;

  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&pr.lock);
}

void
panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}

// snprintf - 格式化输出到缓冲区
static int
sprintint(char *buf, int size, int pos, int xx, int base, int sign)
{
  char tmp[16];
  int i = 0;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  do {
    tmp[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    tmp[i++] = '-';

  while(--i >= 0 && pos < size - 1)
    buf[pos++] = tmp[i];
  
  return pos;
}

int
snprintf(char *buf, int size, const char *fmt, ...)
{
  va_list ap;
  int i, c, pos = 0;
  char *s;

  if(buf == 0 || size <= 0)
    return 0;

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0 && pos < size - 1; i++){
    if(c != '%'){
      buf[pos++] = c;
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      pos = sprintint(buf, size, pos, va_arg(ap, int), 10, 1);
      break;
    case 'x':
      pos = sprintint(buf, size, pos, va_arg(ap, int), 16, 0);
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      while(*s && pos < size - 1)
        buf[pos++] = *s++;
      break;
    case '%':
      buf[pos++] = '%';
      break;
    case '0':
      // 处理 %02d 等格式
      {
        int width = 0;
        while(fmt[i+1] >= '0' && fmt[i+1] <= '9') {
          width = width * 10 + (fmt[++i] - '0');
        }
        c = fmt[++i] & 0xff;
        if(c == 'd') {
          int val = va_arg(ap, int);
          char tmp[16];
          int len = 0, neg = 0;
          uint x;
          if(val < 0) { neg = 1; x = -val; } else { x = val; }
          do { tmp[len++] = digits[x % 10]; } while((x /= 10) != 0);
          if(neg) tmp[len++] = '-';
          while(len < width && pos < size - 1) { buf[pos++] = '0'; width--; }
          while(--len >= 0 && pos < size - 1) buf[pos++] = tmp[len];
        }
      }
      break;
    default:
      buf[pos++] = '%';
      if(pos < size - 1)
        buf[pos++] = c;
      break;
    }
  }
  va_end(ap);
  buf[pos] = '\0';
  return pos;
}
