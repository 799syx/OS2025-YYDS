//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// 从syscall.c导入
int argint(int, int*);
int argaddr(int, uint64*);

// 从sysfile.c导入
int fdalloc(struct file *f);

// 本地argfd实现
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct mbufq rxq;  // a queue of packets waiting to be received
  struct spinlock lock; // protects the rxq
  int type;          // socket type: SOCK_DGRAM or SOCK_STREAM
  int state;         // TCP state (for SOCK_STREAM)
  uint32 seq;        // TCP sequence number
  uint32 ack;        // TCP acknowledgment number
  int listening;     // 1 if listening for connections
  struct sock *pending; // pending connections (for listening socket)
};

static struct spinlock lock;
static struct sock *sockets;

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

void
sockclose(struct sock *si)
{
  struct sock **pos;
  struct mbuf *m;

  // remove from list of sockets
  acquire(&lock);
  pos = &sockets;
  while (*pos) {
    if (*pos == si){
      *pos = si->next;
      break;
    }
    pos = &(*pos)->next;
  }
  release(&lock);

  // free any pending mbufs
  while (!mbufq_empty(&si->rxq)) {
    m = mbufq_pophead(&si->rxq);
    mbuffree(m);
  }

  kfree((char*)si);
}

int
sockread(struct sock *si, uint64 addr, int n)
{
  struct proc *pr = myproc();
  struct mbuf *m;
  int len;

  acquire(&si->lock);
  while (mbufq_empty(&si->rxq) && !pr->killed) {
    sleep(&si->rxq, &si->lock);
  }
  if (pr->killed) {
    release(&si->lock);
    return -1;
  }
  m = mbufq_pophead(&si->rxq);
  release(&si->lock);

  len = m->len;
  if (len > n)
    len = n;
  if (copyout(pr->pagetable, addr, m->head, len) == -1) {
    mbuffree(m);
    return -1;
  }
  mbuffree(m);
  return len;
}

int
sockwrite(struct sock *si, uint64 addr, int n)
{
  struct proc *pr = myproc();
  struct mbuf *m;

  m = mbufalloc(MBUF_DEFAULT_HEADROOM);
  if (!m)
    return -1;

  if (copyin(pr->pagetable, mbufput(m, n), addr, n) == -1) {
    mbuffree(m);
    return -1;
  }
  net_tx_udp(m, si->raddr, si->lport, si->rport);
  return n;
}

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  struct sock *si;

  acquire(&lock);
  si = sockets;
  while (si) {
    if (si->raddr == raddr && si->lport == lport && si->rport == rport)
      goto found;
    // 对于绑定的socket，只检查本地端口
    if (si->listening && si->lport == lport && si->raddr == 0)
      goto found;
    si = si->next;
  }
  release(&lock);
  mbuffree(m);
  return;

found:
  acquire(&si->lock);
  mbufq_pushtail(&si->rxq, m);
  wakeup(&si->rxq);
  release(&si->lock);
  release(&lock);
}

// sys_socket: 创建socket
// 参数: domain (AF_INET), type (SOCK_DGRAM/SOCK_STREAM), protocol
uint64 sys_socket(void)
{
  int domain, type, protocol;
  struct file *f;
  struct sock *si;
  int fd;

  if (argint(0, &domain) < 0 || argint(1, &type) < 0 || argint(2, &protocol) < 0)
    return -1;

  // 只支持 AF_INET
  if (domain != AF_INET)
    return -1;

  // 只支持 UDP 和 TCP
  if (type != SOCK_DGRAM && type != SOCK_STREAM)
    return -1;

  // 分配文件和socket
  if ((f = filealloc()) == 0)
    return -1;
  if ((si = (struct sock*)kalloc()) == 0) {
    fileclose(f);
    return -1;
  }

  // 初始化socket
  si->raddr = 0;
  si->lport = 0;
  si->rport = 0;
  si->type = type;
  si->state = TCP_CLOSED;
  si->seq = 0;
  si->ack = 0;
  si->listening = 0;
  si->pending = 0;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);

  f->type = FD_SOCK;
  f->readable = 1;
  f->writable = 1;
  f->sock = si;

  // 添加到socket列表
  acquire(&lock);
  si->next = sockets;
  sockets = si;
  release(&lock);

  // 分配文件描述符
  if ((fd = fdalloc(f)) < 0) {
    fileclose(f);
    return -1;
  }

  return fd;
}

// sys_bind: 绑定socket到本地端口
// 参数: fd, port
uint64 sys_bind(void)
{
  struct file *f;
  int port;

  if (argfd(0, 0, &f) < 0 || argint(1, &port) < 0)
    return -1;

  if (f->type != FD_SOCK)
    return -1;

  struct sock *si = f->sock;
  
  // 检查端口是否已被使用
  acquire(&lock);
  struct sock *pos = sockets;
  while (pos) {
    if (pos != si && pos->lport == port) {
      release(&lock);
      return -1;  // 端口已被使用
    }
    pos = pos->next;
  }
  
  si->lport = port;
  release(&lock);
  return 0;
}

// sys_listen: 开始监听连接 (TCP)
// 参数: fd, backlog
uint64 sys_listen(void)
{
  struct file *f;
  int backlog;

  if (argfd(0, 0, &f) < 0 || argint(1, &backlog) < 0)
    return -1;

  if (f->type != FD_SOCK)
    return -1;

  struct sock *si = f->sock;
  
  // 必须是TCP socket且已绑定端口
  if (si->type != SOCK_STREAM || si->lport == 0)
    return -1;

  acquire(&si->lock);
  si->listening = 1;
  si->state = TCP_LISTEN;
  release(&si->lock);

  return 0;
}

// sys_accept: 接受连接 (TCP) - 简化实现
// 参数: fd, addr (可为NULL)
uint64 sys_accept(void)
{
  struct file *f;
  uint64 addr;

  if (argfd(0, 0, &f) < 0 || argaddr(1, &addr) < 0)
    return -1;

  if (f->type != FD_SOCK)
    return -1;

  struct sock *si = f->sock;
  
  if (!si->listening)
    return -1;

  // 简化实现：等待数据到来表示有连接
  acquire(&si->lock);
  struct proc *pr = myproc();
  while (mbufq_empty(&si->rxq) && !pr->killed) {
    sleep(&si->rxq, &si->lock);
  }
  release(&si->lock);

  if (pr->killed)
    return -1;

  // 创建新的连接socket
  struct file *newf;
  struct sock *newsi;
  int fd;

  if ((newf = filealloc()) == 0)
    return -1;
  if ((newsi = (struct sock*)kalloc()) == 0) {
    fileclose(newf);
    return -1;
  }

  // 复制原socket信息
  newsi->lport = si->lport;
  newsi->type = SOCK_STREAM;
  newsi->state = TCP_ESTABLISHED;
  newsi->listening = 0;
  initlock(&newsi->lock, "sock");
  mbufq_init(&newsi->rxq);

  // 移动接收队列到新socket
  acquire(&si->lock);
  newsi->rxq = si->rxq;
  mbufq_init(&si->rxq);
  release(&si->lock);

  newf->type = FD_SOCK;
  newf->readable = 1;
  newf->writable = 1;
  newf->sock = newsi;

  acquire(&lock);
  newsi->next = sockets;
  sockets = newsi;
  release(&lock);

  if ((fd = fdalloc(newf)) < 0) {
    fileclose(newf);
    return -1;
  }

  return fd;
}

// sys_send: 发送数据
// 参数: fd, buf, len, flags
uint64 sys_send(void)
{
  struct file *f;
  uint64 buf;
  int len, flags;

  if (argfd(0, 0, &f) < 0 || argaddr(1, &buf) < 0 || 
      argint(2, &len) < 0 || argint(3, &flags) < 0)
    return -1;

  if (f->type != FD_SOCK)
    return -1;

  // 使用现有的sockwrite
  return sockwrite(f->sock, buf, len);
}

// sys_recv: 接收数据
// 参数: fd, buf, len, flags
uint64 sys_recv(void)
{
  struct file *f;
  uint64 buf;
  int len, flags;

  if (argfd(0, 0, &f) < 0 || argaddr(1, &buf) < 0 || 
      argint(2, &len) < 0 || argint(3, &flags) < 0)
    return -1;

  if (f->type != FD_SOCK)
    return -1;

  // 使用现有的sockread
  return sockread(f->sock, buf, len);
}
