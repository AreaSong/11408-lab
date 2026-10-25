#ifndef RECUR_H
#define RECUR_H

#include <stdbool.h>
#include <stdio.h>

/* 用顺序栈模拟阶乘的递归。0! = 1。n 从 0 到 FACT_MAX，20! 仍放得进 long long。
 * 活动记录里要记的是参数 n，所以直接压进顺序栈：先压 n、n-1、…，再乘着弹出。
 * 这和函数调用栈同一方向。右边是当前调用。max_depth 是同时存在的活动记录数。 */
#define FACT_MAX 20

typedef struct {
    long long value;
    int max_depth;
    bool ok;
} FactResult;

bool fact_by_stack(int n, FILE *out, FactResult *result);

/* 不用栈的写法，结果应与 fact_by_stack 相同。超出 0..FACT_MAX 返回 -1。 */
long long fact_loop(int n);

#endif
