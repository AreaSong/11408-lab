#ifndef CPU_DECODE_H
#define CPU_DECODE_H

#include "common/mm_types.h"

#include <stddef.h>
#include <stdint.h>

/*
 * 字段位置对所有格式都固定：rd 在 [25:23]，rs/rx 在 [22:20]。
 * B 型没有 rd，cond 占用 [25:22]。illegal 为 1 时其余字段仍可用作诊断。
 */
typedef struct {
    uint32_t word;
    int op;
    int rd;
    int rs;
    int rt;
    int mode;
    int cond;
    int funct;
    int shamt;
    uint16_t imm;
    int illegal;
} CpuInsn;

uint32_t cpu_enc_r(int rd, int rs, int rt, int shamt, int funct);
uint32_t cpu_enc_i(int op, int rd, int rs, uint16_t imm);
uint32_t cpu_enc_m(int op, int rd, int rx, int mode, uint16_t disp);
uint32_t cpu_enc_b(int op, int cond, uint16_t disp);

/* 总能填好 out。合法返回 0，保留位或未定义编码返回 -1。 */
int cpu_decode(uint32_t word, CpuInsn *out);

/* 写成能再汇编回去的规范语句。非法指令返回 -1。pc 用来把分支位移还原成目标地址。 */
int cpu_disasm(uint32_t word, uint32_t pc, char *buf, size_t cap);

#endif
