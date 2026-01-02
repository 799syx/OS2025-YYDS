// System call numbers
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_pipe 4
#define SYS_read 5
#define SYS_kill 6
#define SYS_exec 7
#define SYS_fstat 8
#define SYS_chdir 9
#define SYS_dup 10
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_sleep 13
#define SYS_uptime 14 // 获取系统的启动时间
#define SYS_open 15
#define SYS_write 16
#define SYS_mknod 17
#define SYS_unlink 18
#define SYS_link 19
#define SYS_mkdir 20
#define SYS_close 21
#define SYS_cps 22
#define SYS_trace 23
#define SYS_sysinfo 24
#define SYS_setPriority 25
#define SYS_execve 26
#define SYS_getparentpid 27  // 获取当前进程的父进程的pid
#define SYS_print_pgtable 28 // 打印当前进程的页表
#define SYS_mmap 29          // 建立内存文件映射
#define SYS_munmap 30        // 取消内存文件映射
#define SYS_sh_var_read 31   // 信号量：访问共享变量
#define SYS_sh_var_write 32  // 信号量：修改共享变量
#define SYS_sem_create 33    // 信号量：创建信号量
#define SYS_sem_free 34      // 信号量：释放信号量
#define SYS_sem_p 35         // 信号量：P操作，获取资源
#define SYS_sem_v 36         // 信号量：V操作，释放资源
#define SYS_symlink 37       // 创建软链接
#define SYS_mkf 38           // 创建文件
#define SYS_shmgetat 39      // 共享内存
#define SYS_shmrefcount 40   // 共享内存
#define SYS_getcwd 41
#define SYS_dup_new 42
#define SYS_sigalarm 43
#define SYS_sigreturn 44
#define SYS_connect 45
#define SYS_mqget 46
#define SYS_msgsnd 47
#define SYS_msgrcv 48
#define SYS_chmod 49       // 修改文件权限
#define SYS_geti 50         // 保存文件的索引信息
#define SYS_recoveri 51     // 根据文件的索引信息恢复文件
#define SYS_clone 52        // 创建线程
#define SYS_join 53         // 回收线程
#define SYS_rename 54       // 重命名文件
#define SYS_lseek 55        // 文件偏移定位
#define SYS_waitpid 56      // 等待指定子进程
#define SYS_getuid 57       // 获取用户ID
#define SYS_setuid 58       // 设置用户ID
#define SYS_reboot 59       // 系统重启
#define SYS_readdir 60      // 读取目录内容
#define SYS_access 61       // 检查文件访问权限
#define SYS_umask 62        // 设置文件创建掩码
#define SYS_chown 63        // 修改文件所有者
#define SYS_flock 64        // 文件锁定
#define SYS_signal 65       // 注册信号处理函数
#define SYS_pause 66        // 等待信号
#define SYS_sigkill 67      // 发送信号给进程
#define SYS_send 68         // 发送数据到socket
#define SYS_recv 69         // 从socket接收数据
#define SYS_bind 70         // 绑定socket到端口
#define SYS_listen 71       // 监听连接
#define SYS_accept 72       // 接受连接
#define SYS_socket 73       // 创建socket
#define SYS_gettime 74      // 获取系统时间
#define SYS_gethostname 75  // 获取主机名
#define SYS_sethostname 76  // 设置主机名
#define SYS_getenv 77       // 获取环境变量
#define SYS_setenv 78       // 设置环境变量
#define SYS_procinfo 79     // 获取进程信息 (/proc)
#define SYS_kstat 80        // 获取文件状态（不需要打开）
#define SYS_truncate 81     // 截断文件到指定长度
#define SYS_dup2 82         // 复制文件描述符到指定位置
#define SYS_getdents 83     // 获取目录项
#define SYS_nice 84         // 调整进程优先级
#define SYS_times 85        // 获取进程CPU时间统计
#define SYS_getppid 86      // 获取父进程ID
#define SYS_setpgid 87      // 设置进程组ID
#define SYS_getpgid 88      // 获取进程组ID
#define SYS_setsid 89       // 创建新会话
#define SYS_getsid 90       // 获取会话ID
#define SYS_getgid 91       // 获取组ID
#define SYS_setgid 92       // 设置组ID
#define SYS_login 93        // 用户登录认证
#define SYS_sudo 94         // 权限提升
#define SYS_consctl 95      // 控制台模式控制