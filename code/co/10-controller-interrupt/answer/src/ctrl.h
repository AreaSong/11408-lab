#ifndef CTRL_H
#define CTRL_H

#include <stddef.h>
#include <stdint.h>

/*
 * 本关玩具机自己的操作码，编号和第 09 关不同。
 * 取指三拍的顺序与第 09 关相同，这里多了除法、陷阱、中断返回和硬件故障。
 */
enum {
    OP_NOP = 0,
    OP_LDA = 1,
    OP_STA = 2,
    OP_ADD = 3,
    OP_DIV = 4,
    OP_JMP = 5,
    OP_TRAP = 6,
    OP_HLT = 7,
    OP_IRET = 8,
    OP_FAIL = 9
};

/* 直接编码时从低位到高位的顺序，打印也按这个顺序，对照真值表不用重排。 */
#define SIG_PCOUT  (1u << 0)
#define SIG_MARIN  (1u << 1)
#define SIG_MEMR   (1u << 2)
#define SIG_MDRINE (1u << 3)
#define SIG_PCPLUS (1u << 4)
#define SIG_MDROUT (1u << 5)
#define SIG_IRIN   (1u << 6)
#define SIG_IROUT  (1u << 7)
#define SIG_ACCIN  (1u << 8)
#define SIG_ACCOUT (1u << 9)
#define SIG_AIN    (1u << 10)
#define SIG_ADD    (1u << 11)
#define SIG_DIV    (1u << 12)
#define SIG_MDRIN  (1u << 13)
#define SIG_MDROUTE (1u << 14)
#define SIG_MEMW   (1u << 15)
#define SIG_PCIN   (1u << 16)
#define SIG_SAVEPC (1u << 17)
#define SIG_IFCLR  (1u << 18)
#define SIG_VEC    (1u << 19)
#define SIG_SPCOUT (1u << 20)
#define SIG_IFSET  (1u << 21)
#define SIG_N 22

/* 内总线驱动。MDRoutE 的 E 表示数据总线，和第 09 关一样不计入。 */
#define SIG_OUTS (SIG_PCOUT | SIG_MDROUT | SIG_IROUT | SIG_ACCOUT | SIG_SPCOUT)

enum { MEM_WORDS = 32 };

int ceil_log2(int n);
int field_bits(int commands);
int out_count(uint32_t sig);
const char *op_name(int op);
void format_sigs(char *buf, size_t n, uint32_t sig);
void format_inst(char *buf, size_t n, uint16_t word);

#endif
