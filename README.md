# YYDS-OS（参赛方向：OS原理赛道——小型内核实现）

## **项目简介**

本项目是一个基于xv6-RISCV实现的小型OS内核，旨在开发过程中对xv6的各模块进行改进和优化。在原有基础上，我们分别在进程调度、内存管理、文件管理、网络设备、用户管理、Shell增强等多个方面完善了功能，并融合了 **Android**、**Linux**、**HarmonyOS** 等主流操作系统的优秀特性。截至目前共**144个系统调用**（xv6自带21个），为用户提供了更丰富的系统服务。

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

- **系统调用：** 用于支持相应功能以及提供用户接口，共144个（xv6自带21个）

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
  - **Embassy异步调度器**：4级优先级异步任务调度
  - **QoS服务质量调度**（HarmonyOS特性）：6级服务质量等级
  - **Binder IPC**（Android特性）：高效进程间通信
  - **Ability应用框架**（HarmonyOS特性）：Page/Service/Data能力管理

- **内存管理**

  - 写时复制(Copy On Write)
  - 懒分配
  - 基于VMA的文件内存映射(MMAP)
  - 空闲页面链表互斥锁的细粒度化
  - **Slab分配器**：高效的小对象内存分配
  - **VMA管理增强**：更灵活的虚拟内存区域管理
  - **cgroups资源控制**（Linux特性）：CPU、内存、I/O、PID限制

- **文件系统**

  - 三级间接块的混合索引分配方式
  - buffer cache互斥锁的细粒度化
  - 文件访问控制权限
  - 基于索引信息的文件恢复策略
  - **文件系统增强**：kstat、truncate、dup2、getdents
  - **sysfs虚拟文件系统**：暴露内核信息（/sys）
  - **HMDFS分布式文件系统**（HarmonyOS特性）：设备发现、文件共享、同步、冲突处理

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
  - 自动化测试脚本：`python3 run_all_tests.py`
  - 测试结果：**22个测试项，17个PASSED，0个FAILED**

------

## 多系统特性融合

YYDS-OS 融合了多个主流操作系统的优秀特性：

| 来源 | 特性 | 说明 |
|------|------|------|
| 🤖 **Android** | Binder IPC | 高效的进程间通信机制 |
| 🤖 **Android** | 进程冻结 (Freezer) | 后台进程管理，低内存自动冻结 |
| 🐧 **Linux** | cgroups | 资源控制组（CPU、内存、I/O、PID限制） |
| � **Linux** | 权能系统 (Capability) | 细粒度权限控制，30种权能位 |
| 🐧 **Linux** | CPU亲和性 | SMP多核调度优化，负载均衡 |
| 🐧 **Linux** | Futex | 快速用户态互斥锁 |
| 🐧 **Linux** | 实时调度 | SCHED_FIFO/RR/DEADLINE (EDF算法) |
| �🔷 **HarmonyOS** | Ability框架 | Page/Service/Data应用能力管理 |
| 🔷 **HarmonyOS** | Ability IPC | Ability间消息通信和生命周期管理 |
| 🔷 **HarmonyOS** | HMDFS | 分布式文件系统，跨设备同步 |
| 🔷 **HarmonyOS** | QoS调度 | 6级服务质量调度 |

------

## 高级内核功能

### 调度增强
- **实时调度**: SCHED_FIFO (先进先出)、SCHED_RR (时间片轮转)
- **Deadline调度**: EDF (最早截止时间优先) 算法
- **CPU亲和性**: 进程绑定CPU、负载均衡策略

### 资源控制增强
- **IO带宽限制**: 读写速率限制、IOPS限制、Token Bucket算法
- **网络带宽限制**: 发送/接收速率限制、包速率限制

### 进程管理增强
- **进程快照**: Checkpoint/Restore机制，保存和恢复进程状态
- **进程冻结**: Android风格的后台进程管理
- **权能系统**: Linux CAP_* + 鸿蒙风格扩展权能

### 文件系统增强
- **文件版本历史**: 版本创建、恢复、差异比较、自动清理
- **HMDFS同步**: 同步事件队列、全量/增量同步

### 同步原语
- **Futex**: 快速用户态互斥锁，减少系统调用开销

### 性能分析
- **内核分析器**: 上下文切换统计、系统调用统计、采样记录

