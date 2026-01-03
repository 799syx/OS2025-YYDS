// 操作系统特性综合测试程序
// 测试 Binder IPC, cgroups, Ability 框架

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "user/user.h"

void test_binder(void)
{
    printf("\n=== Binder IPC 测试 (Android风格) ===\n");
    
    // 注册服务
    printf("注册服务...\n");
    int handle1 = binder_register("audio_service", 0x1000);
    printf("注册 audio_service: handle=%d\n", handle1);
    
    int handle2 = binder_register("video_service", 0x2000);
    printf("注册 video_service: handle=%d\n", handle2);
    
    int handle3 = binder_register("camera_service", 0x3000);
    printf("注册 camera_service: handle=%d\n", handle3);
    
    // 查找服务
    printf("\n查找服务...\n");
    int found = binder_lookup("audio_service");
    printf("查找 audio_service: handle=%d\n", found);
    
    // 列出服务
    printf("\n当前服务列表:\n");
    char buf[1024];
    int n = binder_list(buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 释放服务
    printf("\n释放 video_service...\n");
    binder_release(handle2);
    
    // 打印统计
    binder_stats();
    
    printf("Binder IPC 测试完成!\n");
}

void test_cgroups(void)
{
    printf("\n=== cgroups 测试 (Linux风格) ===\n");
    
    // 创建 cgroup
    printf("创建 cgroup...\n");
    int cg1 = cgroup_create("app_group", 0);
    printf("创建 app_group: id=%d\n", cg1);
    
    int cg2 = cgroup_create("system_group", 0);
    printf("创建 system_group: id=%d\n", cg2);
    
    // 设置资源限制
    printf("\n设置资源限制...\n");
    cgroup_set_memory(cg1, 64 * 1024 * 1024);  // 64MB
    printf("app_group 内存限制: 64MB\n");
    
    cgroup_set_cpu(cg1, 512);  // CPU 份额
    printf("app_group CPU 份额: 512\n");
    
    cgroup_set_memory(cg2, 128 * 1024 * 1024);  // 128MB
    printf("system_group 内存限制: 128MB\n");
    
    // 将当前进程加入 cgroup
    printf("\n将当前进程加入 cgroup...\n");
    cgroup_attach(cg1, getpid());
    printf("进程 %d 加入 app_group\n", getpid());
    
    // 列出 cgroups
    printf("\n当前 cgroups:\n");
    char buf[1024];
    int n = cgroup_list(buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 打印统计
    cgroups_stats();
    
    printf("cgroups 测试完成!\n");
}

void test_ability(void)
{
    printf("\n=== Ability 框架测试 (鸿蒙风格) ===\n");
    
    // 注册 Ability
    printf("注册 Ability...\n");
    int ab1 = ability_register("com.yyds.launcher", "MainAbility", ABILITY_PAGE);
    printf("注册 MainAbility (PAGE): id=%d\n", ab1);
    
    int ab2 = ability_register("com.yyds.launcher", "SettingsAbility", ABILITY_PAGE);
    printf("注册 SettingsAbility (PAGE): id=%d\n", ab2);
    
    int ab3 = ability_register("com.yyds.music", "MusicService", ABILITY_SERVICE);
    printf("注册 MusicService (SERVICE): id=%d\n", ab3);
    
    // 启动 Ability
    printf("\n启动 Ability...\n");
    ability_start(ab1);
    printf("启动 MainAbility\n");
    
    ability_start(ab2);
    printf("启动 SettingsAbility\n");
    
    ability_start(ab3);
    printf("启动 MusicService\n");
    
    // 列出 Abilities
    printf("\n当前 Abilities:\n");
    char buf[1024];
    int n = ability_list(buf, sizeof(buf));
    if (n > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }
    
    // 页面导航
    printf("\n页面导航测试...\n");
    printf("返回上一页...\n");
    ability_back();
    
    // 停止 Ability
    printf("\n停止 MusicService...\n");
    ability_stop(ab3);
    
    // 打印统计
    ability_stats();
    
    printf("Ability 框架测试完成!\n");
}

int main(int argc, char *argv[])
{
    printf("================================================\n");
    printf("    YYDS-OS 多系统特性测试\n");
    printf("    Android + Linux + HarmonyOS 特性集成\n");
    printf("================================================\n");
    
    test_binder();
    test_cgroups();
    test_ability();
    
    printf("\n================================================\n");
    printf("    所有测试完成!\n");
    printf("================================================\n");
    
    exit(0);
}
