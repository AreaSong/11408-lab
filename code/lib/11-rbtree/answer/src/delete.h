#ifndef DELETE_H
#define DELETE_H

#include "tree.h"

#include <stdio.h>

/* 删掉返回 0，没有这个关键字返回 1。双孩子用中序后继顶上。 */
int rb_delete(RbTree *t, int key, FILE *out, int *spins);

#endif
