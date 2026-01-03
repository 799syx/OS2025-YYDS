#include "types.h"
#include "riscv.h"
#include "defs.h" //存放函数声明
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

int sh_var_for_sem_demo = 0; // 信号量；共享变量

uint64 sys_setPriority(void)
{
  int pid, priority;

  // 从用户栈中读取 pid 和 priority 参数
  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
  {
    return -1; // 参数读取失败，返回错误
  }

  // 调用 setPriority 函数设置进程优先级
  return setPriority(pid, priority);
}

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;

  struct proc *p = myproc();
  addr = p->sz;
  uint64 sz = p->sz;

  if (n > 0)
  {
    // 懒分配
    p->sz += n;
  }
  else if (sz + n > 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
    p->sz = sz;
  }
  else
  {
    return -1;
  }
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_cps(void)
{
  return cps();
}

uint64 sys_trace(void)
{ // 为当前进程的trace_mask赋值
  int n;
  if (argint(0, &n) < 0)
  { // n赋值为p->trapframe->a0，a0来自于进程用户空间，用与传参
    return -1;
  }
  myproc()->trace_mask = n; // trace_mask保存了a0的信息，用于调试
  return 0;
}

uint64 sys_sysinfo(void)
{
  struct sysinfo info;
  freebytes(&info.freemem);
  procnum(&info.nproc);

  // 获取虚拟地址
  uint64 dstaddr;
  argaddr(0, &dstaddr);

  // 从内核空间拷贝数据到用户空间
  if (copyout(myproc()->pagetable, dstaddr, (char *)&info, sizeof info) < 0)
    return -1;

  return 0;
}

uint64
sys_execve(void)
{
  char path[260], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;
  // 获取路径和参数地址
  if (argstr(0, path, 260) < 0 || argaddr(1, &uargv) < 0)
  {
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  // 设置 argv[0] 为程序的路径
  argv[0] = path;
  // 从用户空间获取其他参数
  for (i = 1;; i++)
  {
    if (i >= NELEM(argv))
      goto bad;
    // 获取每个参数的地址
    if (fetchaddr(uargv + sizeof(uint64) * (i - 1), (uint64 *)&uarg) < 0)
      goto bad;
    // 如果参数为空，结束循环
    if (uarg == 0)
    {
      argv[i] = 0;
      break;
    }
    // 为参数分配内存
    argv[i] = kalloc();
    if (argv[i] == 0)
      goto bad;
    // 获取字符串内容
    if (fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }
  // 打印参数（调试用）
  // for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
  // {
  //   printf("argv[%d]: %s\n", i, argv[i]);
  // }
  // 调用 exec 执行程序
  // printf("%s", path);
  int ret = exec(path, argv);
  // 清理已分配的内存
  for (i = 1; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return ret;
bad:
  // 清理已分配的内存
  for (i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64 sys_getparentpid(void)
{
  struct proc *p = myproc();
  return p->parent->pid;
}

uint64 sys_print_pgtable(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  vmprint(p->pagetable);
  release(&p->lock);
  return 0;
}

// 信号量
int sys_sh_var_read()
{
  return sh_var_for_sem_demo;
}
// 信号量
int sys_sh_var_write()
{
  int n;
  if (argint(0, &n) < 0)
  {
    return -1;
  }
  sh_var_for_sem_demo = n;
  return sh_var_for_sem_demo;
}

// 信号量：创建信号量
int sys_sem_create()
{
  int n_sem, id;
  if (argint(0, &n_sem) < 0 || n_sem <= 0) // 参数必须合法且大于0
  {
    return -1;
  }

  for (id = 0; id < SEM_MAX_NUM; id++)
  {
    acquire(&sems[id].lock);
    if (sems[id].allocated == 0)
    {
      sems[id].allocated = 1;
      sems[id].resource_count = n_sem; // 分配资源
      sem_used_count++;
      printf("创建了 %d sem\n", id);
      release(&sems[id].lock);
      return id; // 返回信号量索引
    }
    release(&sems[id].lock);
  }

  return -1; // 没有可用的信号量
}
// 信号量：释放信号量
int sys_sem_free()
{
  int id;
  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 检查参数范围
  {
    return -1;
  }

  acquire(&sems[id].lock);
  if (sems[id].allocated == 1)
  {
    sems[id].allocated = 0;
    sems[id].resource_count = 0; // 清除资源计数
    sem_used_count--;
    printf("释放 %d sem\n", id);
  }
  release(&sems[id].lock);

  return 0;
}
// 信号量：P操作，获取资源
int sys_sem_p()
{
  int id;
  struct proc *p = myproc();

  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 参数合法性检查
  {
    return -1;
  }

  // printf("sem_p: 尝试获取信号量 id = %d\n", id);

  acquire(&p->lock);       // 获取当前进程的锁
  acquire(&sems[id].lock); // 获取信号量锁

  sems[id].resource_count--;
  if (sems[id].resource_count < 0)
  {
    release(&sems[id].lock);    // 释放信号量锁
    sleep(&sems[id], &p->lock); // 使用进程锁进行休眠
  }
  else
  {
    release(&sems[id].lock); // 如果资源足够，释放信号量锁
  }

  release(&p->lock); // 释放进程锁
  return 0;
}

// 信号量：V操作，释放资源
int sys_sem_v()
{
  int id;
  if (argint(0, &id) < 0 || id < 0 || id >= SEM_MAX_NUM) // 参数合法性检查
  {
    return -1;
  }

  // printf("sem_v: 尝试释放信号量 id = %d\n", id);

  acquire(&sems[id].lock); // 获取信号量锁

  sems[id].resource_count++;
  if (sems[id].resource_count <= 0)
  {
    wakeup(&sems[id]); // 唤醒等待的进程
  }

  release(&sems[id].lock); // 释放信号量锁
  return 0;
}

uint64 sys_shmgetat(void)
{
  int key, num;
  if (argint(0, &key) < 0 || argint(1, &num) < 0)
    return -1;
  return (uint64)shmgetat(key, num);
}

int sys_shmrefcount(void)
{
  int key;
  if (argint(0, &key) < 0)
  {
    return -1;
  }
  return shmrefcount(key);
}
// sysproc.c
uint64 sys_sigalarm(void) {
  int n;
  uint64 fn;
  if(argint(0, &n) < 0)
    return -1;
  if(argaddr(1, &fn) < 0)
    return -1;
  
  return sigalarm(n, (void(*)())(fn));
}

uint64 sys_sigreturn(void) {
	return sigreturn();
}
int sys_clone(void)
{
  uint64 fcn;
  uint64 arg;
  uint64 stack;
  argaddr(0,&fcn);
  argaddr(1,&arg);
  argaddr(2,&stack);
  return clone(fcn,arg,stack);
}
int sys_join(void)
{
  uint64 stackaddr;
  argaddr(0,&stackaddr);
  return join(stackaddr);
}

// sys_waitpid: 等待指定子进程退出
// 参数: pid (-1表示任意子进程), status地址
uint64 sys_waitpid(void)
{
  int pid;
  uint64 addr;
  if (argint(0, &pid) < 0 || argaddr(1, &addr) < 0)
    return -1;
  return waitpid(pid, addr);
}

// sys_getuid: 获取当前进程的用户ID
uint64 sys_getuid(void)
{
  return myproc()->uid;
}

// sys_setuid: 设置当前进程的用户ID
// 只有root用户(uid=0)可以设置任意uid，普通用户只能设置为自己的uid
uint64 sys_setuid(void)
{
  int uid;
  if (argint(0, &uid) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  // 只有root可以设置任意uid
  if (p->uid != 0 && uid != p->uid)
    return -1;
  
  p->uid = uid;
  return 0;
}

// sys_reboot: 系统重启
// 通过写入QEMU的测试设备来触发重启
uint64 sys_reboot(void)
{
  printf("\n\033[1;31m系统正在重启...\033[0m\n");
  
  // QEMU virt机器的关机/重启地址
  // 写入0x5555表示关机，写入0x7777表示重启
  volatile uint32 *test = (uint32 *)0x100000;
  *test = 0x7777;  // 重启
  
  // 如果上面的方法不工作，尝试另一种方式
  // 通过SBI调用来重启 (RISC-V)
  asm volatile("li a7, 8");  // SBI_SHUTDOWN
  asm volatile("ecall");
  
  return 0;  // 不会到达这里
}

// 信号定义
#define SIGINT    2   // 中断 (Ctrl+C)
#define SIGKILL   9   // 强制终止 (不可捕获)
#define SIGTERM   15  // 终止
#define SIG_DFL   ((void (*)(int))0)  // 默认处理
#define SIG_IGN   ((void (*)(int))1)  // 忽略信号

// sys_signal: 注册信号处理函数
// 参数: signum (信号编号), handler (处理函数地址)
// 返回: 之前的处理函数地址
uint64 sys_signal(void)
{
  int signum;
  uint64 handler;
  
  if (argint(0, &signum) < 0 || argaddr(1, &handler) < 0)
    return -1;
  
  // 检查信号编号有效性
  if (signum < 1 || signum >= 32)
    return -1;
  
  // SIGKILL 不能被捕获或忽略
  if (signum == SIGKILL)
    return -1;
  
  struct proc *p = myproc();
  uint64 old_handler = (uint64)p->signal_handlers[signum];
  p->signal_handlers[signum] = (void (*)())handler;
  
  return old_handler;
}

// sys_pause: 等待信号
// 暂停进程直到收到信号
uint64 sys_pause(void)
{
  struct proc *p = myproc();
  
  acquire(&p->lock);
  p->paused = 1;
  
  // 等待直到收到信号
  while (p->pending_signals == 0 && p->killed == 0) {
    sleep(p, &p->lock);
  }
  
  p->paused = 0;
  release(&p->lock);
  
  // 如果被杀死，返回-1
  if (p->killed)
    return -1;
  
  return 0;
}

// sys_sigkill: 发送信号给进程
// 参数: pid (进程ID), signum (信号编号)
uint64 sys_sigkill(void)
{
  int pid, signum;
  
  if (argint(0, &pid) < 0 || argint(1, &signum) < 0)
    return -1;
  
  // 检查信号编号有效性
  if (signum < 1 || signum >= 32)
    return -1;
  
  struct proc *p;
  int need_wakeup = 0;
  
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid) {
      // 设置待处理信号
      p->pending_signals |= (1 << signum);
      
      // SIGKILL 和 SIGTERM 直接杀死进程
      if (signum == SIGKILL || signum == SIGTERM) {
        p->killed = 1;
      }
      
      // 如果进程在 pause 中等待或在睡眠中，标记需要唤醒
      if (p->paused || p->state == SLEEPING) {
        need_wakeup = 1;
        p->state = RUNNABLE;
      }
      
      release(&p->lock);
      
      // 在释放锁后唤醒
      if (need_wakeup) {
        wakeup(p);
      }
      return 0;
    }
    release(&p->lock);
  }
  
  return -1;  // 进程不存在
}

// =============== 系统工具功能 ===============

// 全局主机名
char hostname[64] = "yyds-os";

// 全局环境变量表
#define MAX_ENV_VARS 32
#define MAX_ENV_NAME 32
#define MAX_ENV_VALUE 128

static struct {
  char name[MAX_ENV_NAME];
  char value[MAX_ENV_VALUE];
  int used;
} env_vars[MAX_ENV_VARS];

// sys_gettime: 获取系统时间 (ticks)
uint64 sys_gettime(void)
{
  return ticks;
}

// sys_gethostname: 获取主机名
uint64 sys_gethostname(void)
{
  uint64 buf;
  int len;
  
  if (argaddr(0, &buf) < 0 || argint(1, &len) < 0)
    return -1;
  
  struct proc *p = myproc();
  int hlen = strlen(hostname);
  if (hlen >= len)
    hlen = len - 1;
  
  if (copyout(p->pagetable, buf, hostname, hlen + 1) < 0)
    return -1;
  
  return 0;
}

// sys_sethostname: 设置主机名
uint64 sys_sethostname(void)
{
  char name[64];
  int len;
  
  if (argstr(0, name, 64) < 0 || argint(1, &len) < 0)
    return -1;
  
  if (len > 63) len = 63;
  memmove(hostname, name, len);
  hostname[len] = 0;
  
  return 0;
}

// 简单字符串比较
static int streq(const char *a, const char *b)
{
  while (*a && *b && *a == *b) { a++; b++; }
  return *a == *b;
}

// sys_getenv: 获取环境变量
uint64 sys_getenv(void)
{
  char name[MAX_ENV_NAME];
  uint64 buf;
  int len;
  
  if (argstr(0, name, MAX_ENV_NAME) < 0 || argaddr(1, &buf) < 0 || argint(2, &len) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  for (int i = 0; i < MAX_ENV_VARS; i++) {
    if (env_vars[i].used && streq(env_vars[i].name, name)) {
      int vlen = strlen(env_vars[i].value);
      if (vlen >= len) vlen = len - 1;
      if (copyout(p->pagetable, buf, env_vars[i].value, vlen + 1) < 0)
        return -1;
      return 0;
    }
  }
  
  return -1;  // 未找到
}

// sys_setenv: 设置环境变量
uint64 sys_setenv(void)
{
  char name[MAX_ENV_NAME];
  char value[MAX_ENV_VALUE];
  
  if (argstr(0, name, MAX_ENV_NAME) < 0 || argstr(1, value, MAX_ENV_VALUE) < 0)
    return -1;
  
  // 查找是否已存在
  for (int i = 0; i < MAX_ENV_VARS; i++) {
    if (env_vars[i].used && streq(env_vars[i].name, name)) {
      safestrcpy(env_vars[i].value, value, MAX_ENV_VALUE);
      return 0;
    }
  }
  
  // 找一个空槽
  for (int i = 0; i < MAX_ENV_VARS; i++) {
    if (!env_vars[i].used) {
      env_vars[i].used = 1;
      safestrcpy(env_vars[i].name, name, MAX_ENV_NAME);
      safestrcpy(env_vars[i].value, value, MAX_ENV_VALUE);
      return 0;
    }
  }
  
  return -1;  // 环境变量表已满
}

// 简单整数转字符串
static int itoa(int n, char *buf)
{
  char tmp[12];
  int i = 0, j = 0;
  int neg = 0;
  
  if (n < 0) { neg = 1; n = -n; }
  if (n == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
  
  while (n > 0) {
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }
  if (neg) buf[j++] = '-';
  while (i > 0) buf[j++] = tmp[--i];
  buf[j] = 0;
  return j;
}

// 追加字符串
static int strapp(char *dst, int off, const char *src)
{
  while (*src) dst[off++] = *src++;
  return off;
}

// sys_procinfo: 获取进程信息 (/proc)
uint64 sys_procinfo(void)
{
  int pid;
  uint64 buf;
  int len;
  
  if (argint(0, &pid) < 0 || argaddr(1, &buf) < 0 || argint(2, &len) < 0)
    return -1;
  
  struct proc *p = myproc();
  char info[512];
  int offset = 0;
  char numbuf[12];
  
  if (pid == -1) {
    // 列出所有进程
    offset = strapp(info, offset, "PID\tSTATE\tNAME\n");
    struct proc *pp;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      acquire(&pp->lock);
      if (pp->state != UNUSED && offset < 480) {
        char *state;
        switch(pp->state) {
          case SLEEPING: state = "SLEEP"; break;
          case RUNNABLE: state = "READY"; break;
          case RUNNING:  state = "RUN"; break;
          case ZOMBIE:   state = "ZOMBIE"; break;
          default:       state = "???"; break;
        }
        itoa(pp->pid, numbuf);
        offset = strapp(info, offset, numbuf);
        offset = strapp(info, offset, "\t");
        offset = strapp(info, offset, state);
        offset = strapp(info, offset, "\t");
        offset = strapp(info, offset, pp->name);
        offset = strapp(info, offset, "\n");
      }
      release(&pp->lock);
    }
  } else {
    // 获取特定进程信息
    struct proc *pp;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      acquire(&pp->lock);
      if (pp->pid == pid && pp->state != UNUSED) {
        offset = strapp(info, offset, "PID: ");
        itoa(pp->pid, numbuf);
        offset = strapp(info, offset, numbuf);
        offset = strapp(info, offset, "\nName: ");
        offset = strapp(info, offset, pp->name);
        offset = strapp(info, offset, "\nState: ");
        itoa(pp->state, numbuf);
        offset = strapp(info, offset, numbuf);
        offset = strapp(info, offset, "\nUID: ");
        itoa(pp->uid, numbuf);
        offset = strapp(info, offset, numbuf);
        offset = strapp(info, offset, "\nParent: ");
        itoa(pp->parent ? pp->parent->pid : 0, numbuf);
        offset = strapp(info, offset, numbuf);
        offset = strapp(info, offset, "\n");
        release(&pp->lock);
        break;
      }
      release(&pp->lock);
    }
    if (offset == 0) return -1;
  }
  
  info[offset] = 0;
  if (offset >= len) offset = len - 1;
  if (copyout(p->pagetable, buf, info, offset + 1) < 0)
    return -1;
  
  return offset;
}

// =============== 进程管理功能 ===============

// sys_nice: 调整进程优先级
// 参数: increment (优先级增量, 正数降低优先级, 负数提高)
uint64 sys_nice(void)
{
  int increment;
  
  if (argint(0, &increment) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  // 调整优先级 (priority越小优先级越高)
  int new_priority = p->priority + increment;
  if (new_priority < 0) new_priority = 0;
  if (new_priority > 20) new_priority = 20;
  
  p->priority = new_priority;
  return p->priority;
}

// 时间统计结构
struct tms {
  uint tms_utime;   // 用户态CPU时间
  uint tms_stime;   // 内核态CPU时间
  uint tms_cutime;  // 子进程用户态时间
  uint tms_cstime;  // 子进程内核态时间
};

// sys_times: 获取进程CPU时间统计
uint64 sys_times(void)
{
  uint64 buf;
  
  if (argaddr(0, &buf) < 0)
    return -1;
  
  struct proc *p = myproc();
  struct tms t;
  
  // 简化实现: 使用cpu_time作为总时间
  t.tms_utime = p->cpu_time;
  t.tms_stime = 0;  // 暂不区分
  t.tms_cutime = 0; // 暂不统计子进程
  t.tms_cstime = 0;
  
  if (copyout(p->pagetable, buf, (char *)&t, sizeof(t)) < 0)
    return -1;
  
  return ticks;
}

// sys_getppid: 获取父进程ID
uint64 sys_getppid(void)
{
  struct proc *p = myproc();
  
  if (p->parent)
    return p->parent->pid;
  return 0;  // init进程没有父进程
}

// sys_setpgid: 设置进程组ID
// 参数: pid (0表示当前进程), pgid (0表示使用pid作为pgid)
uint64 sys_setpgid(void)
{
  int pid, pgid;
  
  if (argint(0, &pid) < 0 || argint(1, &pgid) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  if (pid == 0) pid = p->pid;
  if (pgid == 0) pgid = pid;
  
  // 查找目标进程
  for (struct proc *pp = proc; pp < &proc[NPROC]; pp++) {
    acquire(&pp->lock);
    if (pp->pid == pid) {
      // 只能修改自己或子进程的进程组
      if (pp != p && pp->parent != p) {
        release(&pp->lock);
        return -1;
      }
      pp->pgid = pgid;
      release(&pp->lock);
      return 0;
    }
    release(&pp->lock);
  }
  
  return -1;  // 进程不存在
}

// sys_getpgid: 获取进程组ID
// 参数: pid (0表示当前进程)
uint64 sys_getpgid(void)
{
  int pid;
  
  if (argint(0, &pid) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  if (pid == 0)
    return p->pgid;
  
  // 查找目标进程
  for (struct proc *pp = proc; pp < &proc[NPROC]; pp++) {
    acquire(&pp->lock);
    if (pp->pid == pid) {
      int pgid = pp->pgid;
      release(&pp->lock);
      return pgid;
    }
    release(&pp->lock);
  }
  
  return -1;  // 进程不存在
}

// sys_setsid: 创建新会话
// 调用进程成为新会话的首进程和新进程组的组长
uint64 sys_setsid(void)
{
  struct proc *p = myproc();
  
  // 如果已经是进程组组长，不能创建新会话
  if (p->pgid == p->pid) {
    // 检查是否有其他进程在同一进程组
    int group_count = 0;
    for (struct proc *pp = proc; pp < &proc[NPROC]; pp++) {
      acquire(&pp->lock);
      if (pp->pgid == p->pgid && pp->state != UNUSED)
        group_count++;
      release(&pp->lock);
    }
    if (group_count > 1)
      return -1;  // 是进程组组长且有其他成员
  }
  
  // 创建新会话
  p->sid = p->pid;
  p->pgid = p->pid;
  
  return p->sid;
}

// sys_getsid: 获取会话ID
// 参数: pid (0表示当前进程)
uint64 sys_getsid(void)
{
  int pid;
  
  if (argint(0, &pid) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  if (pid == 0)
    return p->sid;
  
  // 查找目标进程
  for (struct proc *pp = proc; pp < &proc[NPROC]; pp++) {
    acquire(&pp->lock);
    if (pp->pid == pid) {
      int sid = pp->sid;
      release(&pp->lock);
      return sid;
    }
    release(&pp->lock);
  }
  
  return -1;  // 进程不存在
}

// =============== 用户/组管理功能 ===============

// sys_getgid: 获取组ID
uint64 sys_getgid(void)
{
  return myproc()->gid;
}

// sys_setgid: 设置组ID
uint64 sys_setgid(void)
{
  int gid;
  
  if (argint(0, &gid) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  // 只有root可以设置任意gid
  if (p->uid != 0 && gid != p->gid)
    return -1;
  
  p->gid = gid;
  return 0;
}

// 简单用户数据库
#define MAX_USERS 8
static struct {
  int uid;
  int gid;
  char username[16];
  char password[16];
  int active;
} users[MAX_USERS] = {
  {0, 0, "root", "root", 1},
  {1000, 1000, "user", "user", 1},
  {1001, 1000, "guest", "guest", 1},
};

// sys_login: 用户登录
uint64 sys_login(void)
{
  char username[16];
  char password[16];
  
  if (argstr(0, username, 16) < 0 || argstr(1, password, 16) < 0)
    return -1;
  
  struct proc *p = myproc();
  
  for (int i = 0; i < MAX_USERS; i++) {
    if (!users[i].active)
      continue;
    
    int match = 1;
    for (int j = 0; j < 16 && users[i].username[j]; j++) {
      if (users[i].username[j] != username[j]) {
        match = 0;
        break;
      }
    }
    if (!match) continue;
    
    match = 1;
    for (int j = 0; j < 16 && users[i].password[j]; j++) {
      if (users[i].password[j] != password[j]) {
        match = 0;
        break;
      }
    }
    
    if (match) {
      p->uid = users[i].uid;
      p->gid = users[i].gid;
      return users[i].uid;
    }
  }
  
  return -1;
}

// sys_sudo: 权限提升
uint64 sys_sudo(void)
{
  char password[16];
  
  if (argstr(0, password, 16) < 0)
    return -1;
  
  int match = 1;
  for (int j = 0; j < 16 && users[0].password[j]; j++) {
    if (users[0].password[j] != password[j]) {
      match = 0;
      break;
    }
  }
  
  if (match) {
    struct proc *p = myproc();
    p->uid = 0;
    p->gid = 0;
    return 0;
  }
  
  return -1;
}

// sys_consctl: 控制台模式控制
// 参数: mode (0=行缓冲, 1=原始模式)
uint64 sys_consctl(void)
{
  int mode;
  
  if (argint(0, &mode) < 0)
    return -1;
  
  consolerawmode(mode);
  return 0;
}