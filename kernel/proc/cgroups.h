// cgroups 头文件

#ifndef _CGROUPS_H_
#define _CGROUPS_H_

#include "types.h"

// 控制器类型
#define CTRL_CPU     0x01
#define CTRL_MEMORY  0x02
#define CTRL_IO      0x04
#define CTRL_PIDS    0x08

// 初始化
void cgroups_init(void);

// cgroup 管理
int cgroup_create(char *name, int parent_id);
int cgroup_delete(int cgroup_id);
int cgroup_attach(int cgroup_id, int pid);
int cgroup_detach(int cgroup_id, int pid);

// 资源限制
int cgroup_set_cpu(int cgroup_id, int shares, int quota, int period);
int cgroup_set_memory(int cgroup_id, uint64 limit);
int cgroup_set_pids(int cgroup_id, int max_pids);

// 资源检查
int cgroup_check_memory(int pid, uint64 size);
void cgroup_update_memory(int pid, int delta);

// 列表和统计
int cgroup_list(char *buf, int len);
void cgroups_print_stats(void);

#endif // _CGROUPS_H_
