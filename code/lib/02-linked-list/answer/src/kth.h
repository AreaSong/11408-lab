#ifndef KTH_H
#define KTH_H

#include <stdio.h>

#include "slist.h"

/* 2009-42：带头结点的单链表，找倒数第 k 个结点（k ≥ 1）。
 * 成功返回 1，并把 data 写入 *value；否则返回 0，不改 *value。
 * 只读链表，不改任何 next。trace 非 NULL 时打印双指针的移动。 */
int list_kth(const SNode *head, int k, int *value, FILE *trace);

#endif
