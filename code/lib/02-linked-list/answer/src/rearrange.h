#ifndef REARRANGE_H
#define REARRANGE_H

#include <stdio.h>

#include "slist.h"

/* 2019-41：把带头结点的单链表 (a1, a2, …, an) 原地重排为
 * (a1, an, a2, an-1, …)。只改 next，不分配新结点。
 * trace 非 NULL 时按官方解答的三步打印切分、逆置和结果。 */
void list_rearrange(SNode *head, FILE *trace);

#endif
