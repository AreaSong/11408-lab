#ifndef INSERT_H
#define INSERT_H

#include "tree.h"

#include <stdio.h>

/* 插入成功返回 0，关键字已存在返回 1。out 为空时不打印。spins 可为空。 */
int rb_insert(RbTree *t, int key, FILE *out, int *spins);

#endif
