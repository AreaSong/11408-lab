#ifndef X86_H
#define X86_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t eax;
    int zf;
    int sf;
    int cf;
    int of;
    int branch;
    int loop;
    int call;
} X86End;

/* lines 不含结束用的「.」。out 为空时只计算结果，不打印。 */
int x86_run(FILE *out, const char *const *lines, int n, X86End *end);

#endif
