// sysfs 头文件

#ifndef _SYSFS_H_
#define _SYSFS_H_

// 初始化 sysfs
void sysfs_init(void);

// 读取 sysfs 文件
int sysfs_read(char *path, char *buf, int len);

// 列出 sysfs 目录
int sysfs_list(char *path, char *buf, int len);

// 检查路径是否是 sysfs 路径
int sysfs_is_sysfs_path(char *path);

#endif // _SYSFS_H_
