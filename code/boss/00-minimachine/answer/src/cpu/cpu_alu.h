#ifndef CPU_ALU_H
#define CPU_ALU_H

#include <stdint.h>

#define CPU_SHL 0
#define CPU_SHR 1
#define CPU_SAR 2

/* 四个标志与 co/02 同一口径：减法的 CF 是借位（补码加法进位取反）。 */
typedef struct {
    uint32_t result;
    int zf;
    int sf;
    int cf;
    int of;
} CpuAlu;

CpuAlu cpu_alu_add(uint32_t a, uint32_t b);
CpuAlu cpu_alu_sub(uint32_t a, uint32_t b);
CpuAlu cpu_alu_logic(uint32_t result);
CpuAlu cpu_alu_shift(uint32_t value, int shamt, int kind, int old_cf);
CpuAlu cpu_alu_mul(uint32_t a, uint32_t b);

/* 成功返回 0。除数为 0，或 80000000H÷(−1)，返回 -1，不写 result。 */
int cpu_alu_div(uint32_t a, uint32_t b, int is_mod, uint32_t *result);

/* cond 见 MM_COND_*。条件成立返回 1。非法条件码返回 0。 */
int cpu_cond(int cond, uint32_t psw);

#endif
