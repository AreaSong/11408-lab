#ifndef SHELL_H
#define SHELL_H

#include "seq.h"

/* 王道默认增量：floor(n/2), floor(n/4), ... , 1。返回增量个数。 */
int shell_halve(int n, int *gaps);

/* 一趟 = 一个增量。组内就是步长为 d 的直接插入。 */
void shell_pass(Seq *s, int d);
void shell_trace(Seq *s, const int *gaps, int ng);

/*
 * 是否存在 ng 个增量，依次各做一趟后与 pass[0..np-1] 相同。
 * 找到时把增量写进 got（可以为 NULL）。n 较小时枚举。
 */
int shell_match(const Seq *init, const Seq *pass, int np, int *got);

#endif
