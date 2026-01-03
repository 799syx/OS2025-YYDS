// Binder IPC 头文件

#ifndef _BINDER_H_
#define _BINDER_H_

#include "types.h"

// 初始化
void binder_init(void);

// 服务管理
int binder_register_service(char *name, uint64 ptr);
int binder_lookup_service(char *name);
int binder_release_service(int handle);

// 事务管理
int binder_call(int handle, int code, void *data, int size, void *reply, int reply_size);
int binder_reply(int trans_id, void *data, int size);
int binder_receive(int *trans_id, int *code, void *data, int *size);

// 列表和统计
int binder_list_services(char *buf, int len);
void binder_print_stats(void);

#endif // _BINDER_H_
