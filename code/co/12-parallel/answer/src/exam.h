#ifndef PARALLEL_EXAM_H
#define PARALLEL_EXAM_H

#include <stdio.h>

/* 认不出的题号返回 NULL。 */
const char *exam_letter(const char *name);
int exam_show(FILE *fp, const char *name);

#endif
