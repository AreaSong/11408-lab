#ifndef OPCODE_H
#define OPCODE_H

#include <stdint.h>
#include <stdio.h>

#define OPCODE_LEVELS 4

typedef struct {
    int naddr;
    int op_bits;
    uint64_t cap;
    long long used;
    uint64_t left;
} ExtLevel;

typedef struct {
    int feasible;
    int asking_max;
    int nlevels;
    int failed;
    ExtLevel lv[OPCODE_LEVELS];
    uint64_t max_count;
    uint64_t used_points;
    uint64_t space;
} ExtPlan;

/* counts 从地址数最多的那一级往下排。最后一项为 -1 时，求这一级最多能有多少条。 */
int ext_solve(ExtPlan *plan, int word, int addr_bits, int top, const long long *counts, int n);

int minlen_calc(int addr_bits, int align_bits, int top, const long long *counts, int n, int *op_bits, int *raw_bits,
                int *aligned_bits);

int show_ext(FILE *out, int word, int addr_bits, int top, const long long *counts, int n);

int show_minlen(FILE *out, int addr_bits, int align_bits, int top, const long long *counts, int n);

const char *opcode_name(int naddr);

#endif
