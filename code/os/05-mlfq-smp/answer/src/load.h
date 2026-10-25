#ifndef LOAD_H
#define LOAD_H

#include "mlfq.h"
#include "smp.h"

/* 读到单独一行 end。失败时已经打印原因，返回 1。 */
int load_mlfq(MlfqJob *jobs, int *n);
int load_smp(SmpJob *jobs, int *n);

#endif
