// Ability 框架头文件

#ifndef _ABILITY_H_
#define _ABILITY_H_

#include "types.h"

// Ability 类型
#define ABILITY_PAGE     1
#define ABILITY_SERVICE  2
#define ABILITY_DATA     3

// 初始化
void ability_init(void);

// Ability 生命周期
int ability_register(char *bundle, char *name, int type);
int ability_start(int ability_id);
int ability_stop(int ability_id);
int ability_destroy(int ability_id);

// 页面导航
int ability_back(void);
int ability_get_foreground(void);

// 列表和统计
int ability_list(char *buf, int len);
void ability_print_stats(void);

#endif // _ABILITY_H_
