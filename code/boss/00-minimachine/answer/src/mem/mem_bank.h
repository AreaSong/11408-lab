#ifndef MEM_BANK_H
#define MEM_BANK_H

#include <stdint.h>
#include <stdio.h>

/* 低位交叉的模块号 = 地址 mod 模块数。地址用题目自己的单位。 */
int mem_bank_mod(int modules, uint32_t addr);

/* 间隔小于模块数、又落在同一模块，才算冲突。left/right 可以是 NULL。 */
int mem_bank_pairs(int modules, const uint32_t *addrs, int n, int *left, int *right, int cap,
                   int *pairs);

/*
 * 连续 words 个字的时间，单位 ns。
 * low = 1 是低位交叉：m×τ ≥ T 时为 T+(n-1)τ，否则按模块空闲时刻排。
 * low = 0 是高位交叉，连续地址都在同一模块。
 */
int mem_seq_ns(int low, int modules, int access_ns, int bus_ns, int words);

void mem_bank_report(FILE *fp, int modules, const uint32_t *addrs, int n);
void mem_seq_report(FILE *fp, int modules, int access_ns, int bus_ns, int words);

#endif
