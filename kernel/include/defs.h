struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct sharemem;
// messagequeue.c
void mqinit();                 // 初始化系统消息队列
int mqget(uint);               // 申请使用某个消息队列
int msgsnd(uint, void *, int); // 发送消息
int msgrcv(uint, void *, int); // 接收消息
void releasemq(uint);          // 释放消息队列
void releasemq2(int);
void addmqcount(uint); // 增加消息队列的引用计数

struct mbuf;
struct sock;

// sharemem.c
void sharememinit();
void *shmgetat(uint64, uint64);
int shmrefcount(uint64 key);
void shmaddcount(uint64 mask);
int shmkeyused(uint64, uint64);
int shmrelease(pagetable_t pagetable, uint64 shm, uint64 keymask);
int allocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void *phyaddr[]);
int shmadd(uint64, uint64, void *physaddr[]);
int deallocshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm);
int mapshm(pagetable_t pagetable, uint64 oldshm, uint64 newshm, uint64 sz, void **physaddr);
int shmrm(int key);

// sysmon.c
void sysmon_init(void);
void sysmon_update(void);
void sysmon_print_summary(void);
void sysmon_print_processes(void);
int sysmon_get_cpu_usage(void);
int sysmon_get_mem_usage(void);
void sysmon_inc_syscalls(void);
void sysmon_inc_interrupts(void);
void sysmon_inc_context_switches(void);

// bio.c
void binit(void);
struct buf *bread(uint, uint);
void brelse(struct buf *);
void bwrite(struct buf *);
void bpin(struct buf *);
void bunpin(struct buf *);

// console.c
void consoleinit(void);
void consoleintr(int);
void consputc(int);
void consolerawmode(int);

// exec.c
int exec(char *, char **);

// file.c
struct file *filealloc(void);
void fileclose(struct file *);
struct file *filedup(struct file *);
void fileinit(void);
int fileread(struct file *, uint64, int n);
int filestat(struct file *, uint64 addr);
int filewrite(struct file *, uint64, int n);

// fs.c
void fsinit(int);
int dirlink(struct inode *, char *, uint);
struct inode *dirlookup(struct inode *, char *, uint *);
struct inode *ialloc(uint, char);
struct inode *idup(struct inode *);
void iinit();
void ilock(struct inode *);
void iput(struct inode *);
void iunlock(struct inode *);
void iunlockput(struct inode *);
void iupdate(struct inode *);
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);
int readi(struct inode *, int, uint64, uint, uint);
void stati(struct inode *, struct stat *);
int writei(struct inode *, int, uint64, uint, uint);
void itrunc(struct inode *);

// ramdisk.c
void ramdiskinit(void);
void ramdiskintr(void);
void ramdiskrw(struct buf *);

// kalloc.c
void *kalloc(void);
void kfree(void *);
void kinit(void);
void freebytes(uint64 *);
int cowpage(pagetable_t, uint64);
void *cowalloc(pagetable_t, uint64);
int krefcnt(void *);
int kaddrefcnt(void *);
// log.c
void initlog(int, struct superblock *);
void log_write(struct buf *);
void begin_op(void);
void end_op(void);

// pipe.c
int pipealloc(struct file **, struct file **);
void pipeclose(struct pipe *, int);
int piperead(struct pipe *, uint64, int);
int pipewrite(struct pipe *, uint64, int);

// printf.c
void printf(char *, ...);
void panic(char *) __attribute__((noreturn));
void printfinit(void);

// proc.c
int setPriority(int pid, int priority);
int cpuid(void);
void exit(int);
int fork(void);
int growproc(int);
pagetable_t proc_pagetable(struct proc *);
void proc_freepagetable(pagetable_t, uint64);
int kill(int);
struct cpu *mycpu(void);
struct cpu *getmycpu(void);
struct proc *myproc();
void procinit(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void setproc(struct proc *);
void sleep(void *, struct spinlock *);
void userinit(void);
int wait(uint64);
int waitpid(int, uint64);
void wakeup(void *);
void wakeupOneProc(void *chan); // 信号量机制需要
void yield(void);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void procdump(void);
int cps(void);
void procnum(uint64 *dst);
int clone(uint64 ,uint64 ,uint64 );
int join(uint64);
// swtch.S
void swtch(struct context *, struct context *);

// spinlock.c
void acquire(struct spinlock *);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
void push_off(void);
void pop_off(void);
void initsem(void); // 信号量：初始化

// sleeplock.c
void acquiresleep(struct sleeplock *);
void releasesleep(struct sleeplock *);
int holdingsleep(struct sleeplock *);
void initsleeplock(struct sleeplock *, char *);

// string.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);

// syscall.c
int argint(int, int *);
int argstr(int, char *, int);
int argaddr(int, uint64 *);
int fetchstr(uint64, char *, int);
int fetchaddr(uint64, uint64 *);
void syscall();

// trap.c
extern uint ticks;
void trapinit(void);
void trapinithart(void);
extern struct spinlock tickslock;
void usertrapret(void);
int mmap_handler(int va, int cause);
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
// uart.c
void uartinit(void);
void uartintr(void);
void uartputc(int);
void uartputc_sync(int);
int uartgetc(void);

// vm.c
void kvminit(void);
void kvminithart(void);
uint64 kvmpa(uint64);
void kvmmap(uint64, uint64, uint64, int);
int mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t uvmcreate(void);
void uvminit(pagetable_t, uchar *, uint);
uint64 uvmalloc(pagetable_t, uint64, uint64);
uint64 uvmdealloc(pagetable_t, uint64, uint64);
int uvmcopy(pagetable_t, pagetable_t, uint64);
void uvmfree(pagetable_t, uint64);
void uvmunmap(pagetable_t, uint64, uint64, int);
void uvmclear(pagetable_t, uint64);
uint64 walkaddr(pagetable_t, uint64);
int copyout(pagetable_t, uint64, char *, uint64);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);
pte_t *walk(pagetable_t, uint64, int);
void vmprint_helper(pagetable_t, int);
void vmprint(pagetable_t);
// plic.c
void plicinit(void);
void plicinithart(void);
int plic_claim(void);
void plic_complete(int);

// virtio_disk.c
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *, int);
void virtio_disk_intr(void);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
// pci.c
void            pci_init();
// e1000.c
void            e1000_init(uint32 *);
void            e1000_intr(void);
int             e1000_transmit(struct mbuf*);

// net.c
void            net_rx(struct mbuf*);
void            net_tx_udp(struct mbuf*, uint32, uint16, uint16);
struct mbuf * mbufalloc(unsigned int headroom);


// sysnet.c
void            sockinit(void);
int             sockalloc(struct file **, uint32, uint16, uint16);
void            sockclose(struct sock *);
int             sockread(struct sock *, uint64, int);
int             sockwrite(struct sock *, uint64, int);
void            sockrecvudp(struct mbuf*, uint32, uint16, uint16);

// proc.c - 实时调度和Deadline调度
int             sched_setscheduler(int pid, int policy, int priority);
int             sched_setdeadline(int pid, uint64 runtime, uint64 deadline, uint64 period);
void            sched_get_stats(uint64 *rt_sched, uint64 *dl_sched, uint64 *dl_miss);

// checkpoint.c - 进程快照
void            checkpoint_init(void);
uint64          checkpoint_create(void);
uint64          checkpoint_create_pid(int pid);
int             checkpoint_restore(uint64 checkpoint_id);
int             checkpoint_delete(uint64 checkpoint_id);
int             checkpoint_delete_all(int pid);
int             checkpoint_list(char *buf, int len);
int             checkpoint_info(uint64 checkpoint_id, int *pid, uint64 *sz, int *num_pages);
int             checkpoint_exists(int pid);
void            checkpoint_print_stats(void);
void            checkpoint_get_stats(uint64 *created, uint64 *restored, uint64 *deleted);

// versioning.c - 文件版本历史
void            versioning_init(void);
int             version_enable(char *path);
int             version_disable(char *path);
int             version_create(char *path, char *comment);
int             version_restore(char *path, int version_num);
int             version_list(char *path, char *buf, int len);
int             version_info(char *path, int version_num, uint64 *timestamp, uint64 *size, char *comment);
int             version_delete(char *path, int version_num);
int             version_cleanup(char *path, int keep_count);
int             version_diff(char *path, int ver1, int ver2, char *buf, int len);
int             version_current(char *path);
int             version_set_config(char *path, int max_versions, int auto_version);
void            versioning_print_stats(void);
void            versioning_get_stats(uint64 *versions, uint64 *restores, uint64 *cleanups);
int             versioning_list_files(char *buf, int len);

// cgroups.c - IO和网络带宽限制
int             cgroup_set_io_limit(int cgroup_id, uint64 read_bps, uint64 write_bps, int weight);
int             cgroup_set_io_iops(int cgroup_id, uint64 read_iops, uint64 write_iops);
int             cgroup_check_io_read(int pid, uint64 bytes);
int             cgroup_check_io_write(int pid, uint64 bytes);
int             cgroup_set_net_limit(int cgroup_id, uint64 tx_bps, uint64 rx_bps, int priority);
int             cgroup_set_net_pps(int cgroup_id, uint64 tx_pps, uint64 rx_pps);
int             cgroup_check_net_tx(int pid, uint64 bytes);
int             cgroup_check_net_rx(int pid, uint64 bytes);
void            cgroup_get_io_stats(int cgroup_id, uint64 *read_bytes, uint64 *write_bytes, uint64 *read_ops, uint64 *write_ops);
void            cgroup_get_net_stats(int cgroup_id, uint64 *tx_bytes, uint64 *rx_bytes, uint64 *tx_packets, uint64 *rx_packets);

// ability.c - Ability间通信和生命周期管理
void            ability_mq_init(void);
int             ability_send_message(int src_id, int dst_id, int type, char *data, int len);
int             ability_recv_message(int ability_id, char *buf, int buflen, int *src_id, int *type);
int             ability_call(int src_id, int dst_id, char *request, int req_len, char *response, int resp_len);
int             ability_reply(int src_id, int dst_id, int request_id, char *data, int len);
int             ability_set_state(int ability_id, int new_state);
int             ability_get_state(int ability_id);
int             ability_connect(int src_id, int dst_id);
int             ability_disconnect(int src_id, int dst_id);
void            ability_get_ipc_stats(uint64 *total_msgs, uint64 *total_reqs, uint64 *total_resps);

// hmdfs.c - 跨设备文件同步增强
void            hmdfs_sync_queue_init(void);
int             hmdfs_add_sync_event(int event_type, char *path, int device_id);
int             hmdfs_process_sync_queue(void);
int             hmdfs_sync_file_to_device(char *path, int device_id);
int             hmdfs_fetch_file(char *path, int device_id);
int             hmdfs_full_sync(void);
int             hmdfs_incremental_sync(void);
int             hmdfs_set_sync_priority(char *path, int priority);
int             hmdfs_get_file_status(char *path, int *sync_state, int *replicas, uint64 *version);
int             hmdfs_transfer_file(int src_device, int dst_device, char *path);
int             hmdfs_list_device_files(int device_id, char *buf, int len);

// capability.c - 进程权能系统
void            capability_init(void);
int             cap_check(int pid, int cap);
int             cap_capable(int cap);
int             cap_set(int pid, int cap_type, uint32 caps);
int             cap_get(int pid, int cap_type, uint32 *caps);
int             cap_raise(int pid, int cap);
int             cap_drop(int pid, int cap);
void            cap_fork_init(struct proc *child, struct proc *parent);
void            cap_exec_init(struct proc *p);
const char*     cap_name(int cap);
void            cap_print_stats(void);
int             cap_list_process(int pid, char *buf, int len);

// cpuaffinity.c - CPU亲和性调度
void            cpuaffinity_init(void);
int             sched_setaffinity(int pid, uint32 mask);
int             sched_getaffinity(int pid, uint32 *mask);
int             cpu_allowed(struct proc *p, int cpu);
int             find_least_loaded_cpu(uint32 mask);
void            load_balance(void);
void            cpu_stats_tick(int cpu, int is_idle);
void            cpu_stats_switch(int cpu, int new_pid);
void            cpu_stats_runqueue(int cpu, int len);
int             lb_set_policy(int policy);
int             lb_set_interval(int interval);
void            cpuaffinity_print_stats(void);
int             cpuaffinity_get_info(char *buf, int len);

// futex.c - 快速用户态互斥锁
void            futex_init(void);
int             futex_wait(uint64 uaddr, uint32 val, uint64 timeout);
int             futex_wake(uint64 uaddr, int nr_wake);
int             futex_wake_bitset(uint64 uaddr, int nr_wake, uint32 bitset);
int             futex_requeue(uint64 uaddr, uint64 uaddr2, int nr_wake, int nr_requeue);
int             sys_futex(uint64 uaddr, int op, uint32 val, uint64 timeout, uint64 uaddr2, uint32 val3);
void            futex_print_stats(void);
int             futex_get_stats(uint64 *waits, uint64 *wakes, uint64 *timeouts);

// freezer.c - 进程冻结/解冻
void            freezer_init(void);
int             freeze_process(int pid, int reason);
int             thaw_process(int pid);
int             freeze_cgroup(int cgroup_id);
int             thaw_cgroup(int cgroup_id);
int             freeze_set_priority(int pid, int prio);
int             auto_freeze_background(void);
int             is_frozen(int pid);
int             frozen_count(void);
int             freezer_set_auto(int enabled);
int             freezer_set_timeout(int timeout);
void            freezer_print_stats(void);
int             freezer_list(char *buf, int len);

// kprofiler.c - 内核性能统计
void            kprofiler_init(void);
void            perf_record(int event, int pid, uint64 data);
void            perf_sched_switch(int from_pid, int to_pid);
void            perf_syscall(int pid, int syscall_num);
int             kprofiler_enable(int enable);
void            kprofiler_print_stats(void);
int             kprofiler_get_stats(char *buf, int len);
