# BirdOS（参赛方向：OS原理赛道——小型内核实现）

## **项目简介**

本项目是一个基于xv6-RISCV实现的小型OS内核，旨在开发过程中对xv6的各模块进行改进和优化。在原有基础上，我们分别在进程调度、内存管理、文件管理、网络设备、用户管理、Shell增强等多个方面完善了功能。截至目前共**95个系统调用**（xv6自带21个），为用户提供了更丰富的系统服务。

**项目成员：** 徐悠然、吴雪琪、孙宇轩

**指导老师：** 田卫东、周红鹃

## 项目组织

```
.
├── Makefile
├── README.md
├── docs			# 说明文档
├── kernel			# 内核代码
│   ├── asm			# 汇编相关
│   ├── driver		# 磁盘驱动以及uart驱动
│   ├── filesystem	# 文件系统
│   ├── include		# 内核头文件
│   ├── interrupt	# 中断
│   ├── kernel.ld	# 链接脚本
│   ├── lib			# 库函数相关
│   ├── lock		# 锁
│   ├── main.c		# 主函数
│   ├── mm			# 内存管理
│   ├── network		# 网卡驱动
│   ├── proc		# 进程管理
│   ├── start.c		
│   ├── syscall.c	# 系统调用接口
│   ├── sysfile.c	# 文件相关系统调用
│   ├── sysnet.c	# 网络相关系统调用
│   └── sysproc.c	# 进程相关系统调用
├── mkfs			# 文件系统初始化
└── user
    ├── program		# 用户命令与程序
    ├── test		# 测试用例
    ├── user.h		# 用户函数库
    └── usys.pl		# 脚本文件
```

------


## 项目运行

### 环境依赖

Ubuntu 20.04	

qemu-5.1.0

RISC-V GNU 编译工具链

------

### 运行命令

- 在项目根目录下通过以下命令构建并运行OS

```
make qemu
```

- 清理内核镜像以及用户编译结果

```
make clean
```

------

## 内核各模块设计综述

在xv6原有基础上，我们针对内核各模块进行了相关改进与创新，添加如下功能：

- **系统调用：** 用于支持相应功能以及提供用户接口，共95个（xv6自带21个）

- **进程管理**

  - 基于动态优先级的进程调度器
  - 共享内存的进程通信方式
  - 消息队列的进程通信方式
  - 基于中断的定时提醒机制
  - 用于进程同步与互斥的记录型信号量
  - 内核多线程与用户线程库
  - **信号系统**：signal、sigkill、pause、Ctrl+C中断
  - **进程组与会话**：setpgid、getpgid、setsid、getsid
  - **进程信息**：nice、times、getppid

- **内存管理**

  - 写时复制(Copy On Write)
  - 懒分配
  - 基于VMA的文件内存映射(MMAP)
  - 空闲页面链表互斥锁的细粒度化

- **文件系统**

  - 三级间接块的混合索引分配方式
  - buffer cache互斥锁的细粒度化
  - 文件访问控制权限
  - 基于索引信息的文件恢复策略
  - **文件系统增强**：kstat、truncate、dup2、getdents

- **网络设备**

  - e1000网卡驱动程序
  - UDP/IP协议通信的简单支持
  - **Socket API**：socket、bind、listen、accept、send、recv

- **用户管理**

  - 用户/组ID管理：getuid、setuid、getgid、setgid
  - 简单登录认证系统：login
  - 权限提升：sudo

- **Shell增强**

  - 命令历史（↑↓键浏览）
  - Tab自动补全
  - 后台任务管理（&、jobs）
  - 内置命令：history、jobs、exit

- **用户程序**

  - Unix常用命令：cp、mv、touch、head、tail、clear、date、ps、env、export、hostname
  - 实用工具：factor（质因数分解）、seq（数字序列）、yes、cal（日历）、banner（ASCII艺术字）

- **系统测试：** 我们在本项目/user/test下添加了对各功能的相关测试

