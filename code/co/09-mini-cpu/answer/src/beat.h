#ifndef BEAT_H
#define BEAT_H

#include <stdio.h>

/* 一个时钟周期里出现在内总线上的传送。驱动源个数从控制信号里数，不另传一份。 */
typedef struct {
    int index;
    const char *stage;
    const char *func;
    const char *detail;
    const char *sig;
} Beat;

typedef struct {
    const char *stage;
    const char *func;
    const char *detail;
    const char *sig;
} Micro;

/* 以 out 结尾、又不是 outE 的信号，才是内总线驱动。MDRoutE 走的是数据总线。 */
int driver_count(const char *sig);
int beat_print(FILE *out, const Beat *beat);
int micro_print(FILE *out, const Micro *list, int n, int start);

#endif
