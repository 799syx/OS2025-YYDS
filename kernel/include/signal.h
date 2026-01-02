// 信号定义
#ifndef _SIGNAL_H_
#define _SIGNAL_H_

// 信号编号
#define SIGHUP    1   // 挂起
#define SIGINT    2   // 中断 (Ctrl+C)
#define SIGQUIT   3   // 退出
#define SIGILL    4   // 非法指令
#define SIGTRAP   5   // 跟踪/断点
#define SIGABRT   6   // 中止
#define SIGBUS    7   // 总线错误
#define SIGFPE    8   // 浮点异常
#define SIGKILL   9   // 强制终止 (不可捕获)
#define SIGUSR1   10  // 用户定义信号1
#define SIGSEGV   11  // 段错误
#define SIGUSR2   12  // 用户定义信号2
#define SIGPIPE   13  // 管道破裂
#define SIGALRM   14  // 定时器
#define SIGTERM   15  // 终止
#define SIGCHLD   17  // 子进程状态改变
#define SIGCONT   18  // 继续执行
#define SIGSTOP   19  // 停止 (不可捕获)
#define SIGTSTP   20  // 终端停止 (Ctrl+Z)

// 特殊信号处理函数值
#define SIG_DFL ((void (*)(int))0)  // 默认处理
#define SIG_IGN ((void (*)(int))1)  // 忽略信号

// 最大信号数
#define NSIG 32

#endif
