// QoS 和新功能测试程序

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

void test_qos(void)
{
    printf("\n=== QoS 调度器测试 ===\n");
    
    // 获取当前 QoS 级别
    int level = qos_get();
    printf("当前 QoS 级别: %d\n", level);
    
    // 设置为用户交互级别
    printf("设置 QoS 为 USER_INTERACTIVE (0)...\n");
    if (qos_set(QOS_USER_INTERACTIVE) == 0) {
        printf("设置成功, 新级别: %d\n", qos_get());
    } else {
        printf("设置失败\n");
    }
    
    // 设置为后台级别
    printf("设置 QoS 为 BACKGROUND (5)...\n");
    if (qos_set(QOS_BACKGROUND) == 0) {
        printf("设置成功, 新级别: %d\n", qos_get());
    }
    
    // 恢复默认
    qos_set(QOS_DEFAULT);
    
    // 打印 QoS 统计
    printf("\nQoS 统计信息:\n");
    qos_print();
    
    printf("QoS 测试通过!\n");
}

void test_sysfs(void)
{
    printf("\n=== sysfs 测试 ===\n");
    
    char buf[512];
    int n;
    
    // 读取内核版本
    printf("读取 /sys/kernel/version:\n");
    n = sysfs_read("/sys/kernel/version", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("  %s", buf);
    } else {
        printf("  读取失败\n");
    }
    
    // 读取主机名
    printf("读取 /sys/kernel/hostname:\n");
    n = sysfs_read("/sys/kernel/hostname", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("  %s", buf);
    }
    
    // 读取运行时间
    printf("读取 /sys/kernel/uptime:\n");
    n = sysfs_read("/sys/kernel/uptime", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("  %s", buf);
    }
    
    // 读取内存信息
    printf("读取 /sys/mem/total:\n");
    n = sysfs_read("/sys/mem/total", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("  总内存: %s", buf);
    }
    
    printf("读取 /sys/mem/free:\n");
    n = sysfs_read("/sys/mem/free", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("  空闲内存: %s", buf);
    }
    
    // 读取 CPU 信息
    printf("读取 /sys/cpu/info:\n");
    n = sysfs_read("/sys/cpu/info", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 列出 /sys 目录
    printf("列出 /sys 目录:\n");
    n = sysfs_list("/sys", buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    printf("sysfs 测试通过!\n");
}

void test_slab(void)
{
    printf("\n=== Slab 分配器测试 ===\n");
    
    // 打印 slab 统计
    printf("Slab 统计信息:\n");
    slab_stats();
    
    printf("Slab 测试通过!\n");
}

void cpu_work(int iterations)
{
    volatile int sum = 0;
    for (int i = 0; i < iterations; i++) {
        sum += i;
    }
}

void test_qos_scheduling(void)
{
    printf("\n=== QoS 调度测试 ===\n");
    
    int pid1 = fork();
    if (pid1 == 0) {
        // 子进程1: 高优先级
        qos_set(QOS_USER_INTERACTIVE);
        printf("子进程 %d: QoS=USER_INTERACTIVE, 开始工作\n", getpid());
        cpu_work(100000);
        printf("子进程 %d: 完成\n", getpid());
        exit(0);
    }
    
    int pid2 = fork();
    if (pid2 == 0) {
        // 子进程2: 低优先级
        qos_set(QOS_BACKGROUND);
        printf("子进程 %d: QoS=BACKGROUND, 开始工作\n", getpid());
        cpu_work(100000);
        printf("子进程 %d: 完成\n", getpid());
        exit(0);
    }
    
    // 等待子进程
    wait(0);
    wait(0);
    
    printf("QoS 调度测试完成!\n");
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("    YYDS-OS 新功能测试程序\n");
    printf("========================================\n");
    
    test_qos();
    test_sysfs();
    test_slab();
    test_qos_scheduling();
    
    printf("\n========================================\n");
    printf("    所有测试完成!\n");
    printf("========================================\n");
    
    exit(0);
}
