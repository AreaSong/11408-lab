#ifndef SUFFIX_H
#define SUFFIX_H

#include "slist.h"

/* 2012-42：两个带头结点单链表的公共后缀起点。
 * 先比表长，长的先走差值，再同步前进。相等看的是结点地址，不是 data。
 * 没有公共结点时返回 NULL。 */
SNode *list_common_suffix(SNode *a, SNode *b);

#endif
