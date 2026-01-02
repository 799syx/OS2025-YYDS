#include "param.h"
struct stat;
struct rtcdate;
struct sysinfo;
// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int *);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);
int cps(void);
int trace(int);
int sysinfo(struct sysinfo *);
int setPriority(int pid, int priority);
int execve(const char *path, char *argv[], char *envp[]);
int getparentpid(void);
int print_pgtable(void);
void *mmap(void *addr, int length, int prot, int flags, int fd, int offset);
int munmap(void *addr, int length);
int sh_var_read(void);
void sh_var_write(int n);
int symlink(const char*,const char*);
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
int connect(uint32, uint16, uint16);
//恢复被删除的文件
int geti(const char*,uint64);
int recoveri(uint,uint64);
// 信号量
int sem_create(int);
int sem_free(int);
int sem_p(int);
int sem_v(int);
int mkf(char *);
// 共享内存
uint64 shmgetat(int, int);
int shmrefcount(int);
// 消息队列
int mqget(uint);               // 申请使用某个消息队列
int msgsnd(uint, void *, int); // 发送消息
int msgrcv(uint, void *, int); // 接收消息
// 文件权限
int chmod(const char*,char);
// 内核线程
int clone(uint64,uint64,uint64);
int join(uint64);
// 新增系统调用
int rename(const char*, const char*);  // 重命名文件
int lseek(int fd, int offset, int whence);  // 文件偏移定位
int waitpid(int pid, int *status);  // 等待指定子进程
int getuid(void);  // 获取用户ID
int setuid(int uid);  // 设置用户ID
int reboot(void);  // 系统重启

// lseek的whence参数
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// 文件系统增强系统调用
int readdir(int fd, void *buf, int count);  // 读取目录内容
int access(const char *pathname, int mode);  // 检查文件访问权限
int umask(int mask);  // 设置文件创建掩码
int chown(const char *pathname, int uid);  // 修改文件所有者
int flock(int fd, int operation);  // 文件锁定

// access的mode参数
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

// flock的operation参数
#define LOCK_SH 1  // 共享锁
#define LOCK_EX 2  // 排他锁
#define LOCK_UN 4  // 解锁
#define LOCK_NB 8  // 非阻塞

// 信号系统调用
void *signal(int signum, void *handler);  // 注册信号处理函数
int pause(void);  // 等待信号
int sigkill(int pid, int signum);  // 发送信号给进程

// 信号编号
#define SIGHUP    1   // 挂起
#define SIGINT    2   // 中断 (Ctrl+C)
#define SIGQUIT   3   // 退出
#define SIGKILL   9   // 强制终止 (不可捕获)
#define SIGUSR1   10  // 用户定义信号1
#define SIGUSR2   12  // 用户定义信号2
#define SIGTERM   15  // 终止

// 特殊信号处理函数值
#define SIG_DFL ((void *)0)  // 默认处理
#define SIG_IGN ((void *)1)  // 忽略信号

// 网络系统调用
int socket(int domain, int type, int protocol);  // 创建socket
int bind(int fd, int port);  // 绑定端口
int listen(int fd, int backlog);  // 监听连接
int accept(int fd, void *addr);  // 接受连接
int send(int fd, const void *buf, int len, int flags);  // 发送数据
int recv(int fd, void *buf, int len, int flags);  // 接收数据

// Socket类型
#define SOCK_DGRAM  1  // UDP
#define SOCK_STREAM 2  // TCP

// 协议族
#define AF_INET 2

// 系统工具
int gettime(void);  // 获取系统时间 (ticks)
int gethostname(char *buf, int len);  // 获取主机名
int sethostname(const char *name, int len);  // 设置主机名
int getenv(const char *name, char *buf, int len);  // 获取环境变量
int setenv(const char *name, const char *value);  // 设置环境变量
int procinfo(int pid, char *buf, int len);  // 获取进程信息 (-1=所有)

// 文件系统增强
int kstat(const char *path, struct stat *st);  // 获取文件状态（不需要打开）
int truncate(const char *path, int length);   // 截断文件到指定长度
int dup2(int oldfd, int newfd);  // 复制文件描述符到指定位置
int getdents(int fd, void *buf, int count);  // 获取目录项

// 进程管理
int nice(int increment);  // 调整进程优先级
int times(void *buf);     // 获取CPU时间统计
int getppid(void);        // 获取父进程ID
int setpgid(int pid, int pgid);  // 设置进程组ID
int getpgid(int pid);     // 获取进程组ID
int setsid(void);         // 创建新会话
int getsid(int pid);      // 获取会话ID

// 用户/组管理
int getgid(void);         // 获取组ID
int setgid(int gid);      // 设置组ID
int login(const char *username, const char *password);  // 用户登录
int sudo(const char *password);  // 权限提升 (需要root密码)
int consctl(int mode);  // 控制台模式 (0=行缓冲, 1=原始模式)

// 时间统计结构
struct tms {
  uint tms_utime;   // 用户态CPU时间
  uint tms_stime;   // 内核态CPU时间
  uint tms_cutime;  // 子进程用户态时间
  uint tms_cstime;  // 子进程内核态时间
};

// 目录项结构
struct linux_dirent {
  uint ino;       // inode 号
  uint off;       // 偏移
  ushort reclen;  // 记录长度
  char name[15];  // 文件名 (DIRSIZ + 1)
};

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void fprintf(int, const char *, ...);
void printf(const char *, ...);
char *gets(char *, int);
uint strlen(const char *);
void *memset(void *, int, uint);
void *malloc(uint);
void free(void *);
int atoi(const char *);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
int statistics(void *buf, int sz);
// uthread.c
int thread_join(void);
int thread_create(void(*start_routine)(void*),void*arg);

