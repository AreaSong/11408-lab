#ifndef PARSE_H
#define PARSE_H

#include "tree.h"

/* 0 成功，-1 括号或颜色不合法，-2 内部结点超过 RB_KEY_MAX。失败时树保持为空。 */
int rb_parse(RbTree *t, const char *s);

#endif
