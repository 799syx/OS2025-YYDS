#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc/embassy.h"
#include "proc/qos.h"
#include "mm/slab.h"
#include "mm/vma.h"
#include "fs/sysfs.h"
#include "fs/hmdfs.h"
#include "proc/binder.h"
#include "proc/cgroups.h"
#include "proc/ability.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main()
{
  if (cpuid() == 0)
  {
    consoleinit();
    printfinit();
    printf("\033[1;36m");
    printf("\n");
    printf("██╗   ██╗██╗   ██╗██████╗ ███████╗       ██████╗ ███████╗\n");
    printf("╚██╗ ██╔╝╚██╗ ██╔╝██╔══██╗██╔════╝      ██╔═══██╗██╔════╝\n");
    printf(" ╚████╔╝  ╚████╔╝ ██║  ██║███████╗█████╗██║   ██║███████╗\n");
    printf("  ╚██╔╝    ╚██╔╝  ██║  ██║╚════██║╚════╝██║   ██║╚════██║\n");
    printf("   ██║      ██║   ██████╔╝███████║      ╚██████╔╝███████║\n");
    printf("   ╚═╝      ╚═╝   ╚═════╝ ╚══════╝       ╚═════╝ ╚══════╝\n");
    printf("\033[1;33m");
    printf("         Welcome to YYDS-OS v2.0 - Your Awesome OS!\n");
    printf("\033[0m\n");
    kinit();            // 初始化内存，将所有可用内存切碎
    kvminit();          // 创建内核页表，完成内核虚拟地址映射
    kvminithart();      // 把内核页表物理地址放入当前CPU核的页表基地寄存器(satp)中
    procinit();         // process table
    trapinit();         // trap vectors
    trapinithart();     // install kernel trap vector
    plicinit();         // set up interrupt controller
    plicinithart();     // ask PLIC for device interrupts
    binit();            // buffer cache
    iinit();            // inode cache
    fileinit();         // file table
    virtio_disk_init(); // emulated hard disk
    pci_init();
    sockinit();
    initsem();          // 信号量数组初始化
    sharememinit();
    mqinit();
    embassy_init();    // Initialize Embassy async scheduler
    slab_init();       // Initialize Slab allocator
    vma_init();        // Initialize VMA manager
    qos_init();        // Initialize QoS scheduler
    sysfs_init();      // Initialize sysfs
    hmdfs_init();      // Initialize HMDFS distributed file system
    binder_init();     // Initialize Binder IPC (Android-style)
    cgroups_init();    // Initialize cgroups (Linux-style)
    ability_init();    // Initialize Ability framework (HarmonyOS-style)
    printf("\033[0m");
    userinit();           // first user process
    __sync_synchronize(); // 防止编译器优化，确保后续的任何操作都是初始化之后进行
    started = 1;
  }
  else
  {
    while (started == 0)
      ;
    __sync_synchronize();
    printf("\033[1;32mhart %d starting\033[0m\n", cpuid());
    kvminithart();  // turn on paging
    trapinithart(); // install kernel trap vector
    plicinithart(); // ask PLIC for device interrupts
  }

  scheduler();
}
