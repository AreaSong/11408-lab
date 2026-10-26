#ifndef NEST_H
#define NEST_H

#include <stdio.h>

#define NEST_MAX 8
#define NEST_EV 32

/*
 * 响应优先级 rank 越小越先被响应，由硬件排队决定，运行中不变。
 * mask 的第 i 位为 1 表示屏蔽第 i 个中断源（i 从 0 起，和输入顺序一致）。
 * 正在执行的服务程序用自己的屏蔽字：1 挡住的请求不能嵌套，0 的可以。
 * 主程序谁也不屏蔽。同一时刻有多个能进来的请求时，再按 rank 挑一个。
 */
typedef struct {
    char name[16];
    int rank;
    unsigned mask;
} NestSrc;

typedef struct {
    int t;
    int who;
} NestEv;

/* rank[i] 越小处理优先级越高。屏蔽自己和所有不高于自己的中断源。 */
unsigned mask_for(const int *rank, int n, int self);
void mask_text(unsigned mask, int n, char *buf, size_t nbuf);

/*
 * 请求保持到被响应。服务被打断后剩余时间还在。
 * order 收到完成次序。out 为 NULL 时不打印。成功返回 0。
 */
int nest_run(const NestSrc *src, int n, int service, const NestEv *ev, int ne,
             char order[][16], int *norder, FILE *out);

int nest_command(const char *cmd);

#endif
