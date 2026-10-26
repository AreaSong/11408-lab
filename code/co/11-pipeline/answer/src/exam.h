#ifndef PIPE_EXAM_H
#define PIPE_EXAM_H

#include "pipe.h"

#include <stdio.h>

/* 已知的选择题返回 'A' 到 'D'，综合题或练习返回 0。 */
int exam_letter(const char *name);

/* 按内部名字跑一段固定程序，供单元测试核对周期和阻塞。成功返回 0。 */
int exam_sim(const char *key, PipeStat *st);

/* 打印一道题的推导。认识这个名字返回 0，否则返回 1。 */
int show_exam(FILE *fp, const char *name);

#endif
