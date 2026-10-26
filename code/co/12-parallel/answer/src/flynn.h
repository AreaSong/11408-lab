#ifndef PARALLEL_FLYNN_H
#define PARALLEL_FLYNN_H

#include <stdio.h>

/* 返回这一类对应本关哪一步。"无" 表示没有实验。认不出返回 NULL。 */
const char *flynn_step(const char *name);
int flynn_show(FILE *fp, const char *name);
void flynn_map(FILE *fp);

#endif
