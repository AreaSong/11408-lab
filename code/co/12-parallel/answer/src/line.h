#ifndef PARALLEL_LINE_H
#define PARALLEL_LINE_H

#include <stdio.h>

/* 同一地址是真共享；地址不同但行号相同是伪共享；行号不同则各占一行。 */
enum {
    LINE_TRUE = 1,
    LINE_FALSE = 2,
    LINE_NONE = 3
};

int line_kind(unsigned long long size, unsigned long long a, unsigned long long b);
void line_show(FILE *fp, unsigned long long size, unsigned long long a, unsigned long long b);

#endif
