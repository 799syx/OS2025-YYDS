// HMDFS 分布式文件系统测试程序

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

void test_device_management(void)
{
    printf("\n=== HMDFS 设备管理测试 ===\n");
    
    // 注册模拟设备
    printf("注册模拟设备...\n");
    
    int dev1 = hmdfs_register_device("phone-1", 
        "11111111-1111-1111-1111-111111111111", DEV_TYPE_PHONE);
    printf("注册手机设备: id=%d\n", dev1);
    
    int dev2 = hmdfs_register_device("tablet-1",
        "22222222-2222-2222-2222-222222222222", DEV_TYPE_TABLET);
    printf("注册平板设备: id=%d\n", dev2);
    
    int dev3 = hmdfs_register_device("pc-1",
        "33333333-3333-3333-3333-333333333333", DEV_TYPE_PC);
    printf("注册电脑设备: id=%d\n", dev3);
    
    // 列出设备
    printf("\n当前设备列表:\n");
    char buf[1024];
    int n = hmdfs_list_devices(buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 设备下线
    printf("\n设备 %d 下线...\n", dev2);
    hmdfs_device_offline(dev2);
    
    printf("设备管理测试完成!\n");
}

void test_file_sharing(void)
{
    printf("\n=== HMDFS 文件共享测试 ===\n");
    
    // 创建测试文件
    int fd = open("hmdfs_test.txt", 0x200 | 0x001);  // O_CREATE | O_WRONLY
    if (fd >= 0) {
        char *content = "Hello from HMDFS!\n";
        write(fd, content, strlen(content));
        close(fd);
        printf("创建测试文件: hmdfs_test.txt\n");
    }
    
    // 共享文件
    printf("共享文件...\n");
    int share1 = hmdfs_share("hmdfs_test.txt");
    printf("共享 hmdfs_test.txt: slot=%d\n", share1);
    
    int share2 = hmdfs_share("README.md");
    printf("共享 README.md: slot=%d\n", share2);
    
    // 列出共享文件
    printf("\n当前共享文件:\n");
    char buf[1024];
    int n = hmdfs_list_shared(buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 同步
    printf("\n触发同步...\n");
    int synced = hmdfs_sync();
    printf("同步了 %d 个文件\n", synced);
    
    // 取消共享
    printf("\n取消共享 hmdfs_test.txt...\n");
    hmdfs_unshare("hmdfs_test.txt");
    
    printf("文件共享测试完成!\n");
}

void test_statistics(void)
{
    printf("\n=== HMDFS 统计信息 ===\n");
    hmdfs_stats();
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("    HMDFS 分布式文件系统测试\n");
    printf("========================================\n");
    
    test_device_management();
    test_file_sharing();
    test_statistics();
    
    printf("\n========================================\n");
    printf("    HMDFS 测试完成!\n");
    printf("========================================\n");
    
    exit(0);
}
