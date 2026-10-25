#ifndef STATICLIST_H
#define STATICLIST_H

#include <stdbool.h>
#include <stdio.h>

/* 静态链表：数组单元模拟结点，next 存的是下一个单元的下标（游标），不是地址。
 * 容量固定。下标 0 固定当头结点，不存元素。
 * 游标 -1 表示空。不能用 0 当空：0 已经是头结点的下标。
 * 严蔚敏把 0 同时当作空指针和备用链表头，数据表头放在最后一个单元；
 * 王道的图是 0 号当头结点、链尾用空游标。本关按王道的图，另用 spare 指向备用链表，
 * 避免“0 既是下标又是空”撞在一起。
 * 下标 1..STATIC_CAP-1 一开始串成备用链表。插入从 spare 取一个下标，
 * 删除把头插还回去，所以刚释放的下标会被下一次插入立刻用掉。 */
#define STATIC_CAP 8
#define STATIC_NULL (-1)

typedef struct {
    int data;
    int next;
} StaticNode;

typedef struct {
    StaticNode node[STATIC_CAP];
    int spare;
    FILE *trace;
} StaticList;

/* 0 成功，1 位序非法，2 备用链表已空（表满）。 */
#define STATIC_OK 0
#define STATIC_RANGE 1
#define STATIC_FULL 2

void staticlist_init(StaticList *L);

bool staticlist_empty(const StaticList *L);

int staticlist_length(const StaticList *L);

int staticlist_insert(StaticList *L, int i, int e);

bool staticlist_delete(StaticList *L, int i, int *out);

int staticlist_locate(const StaticList *L, int e);

bool staticlist_get(const StaticList *L, int i, int *out);

void staticlist_print(const StaticList *L, FILE *out);

void staticlist_dump(const StaticList *L, FILE *out);

#endif
