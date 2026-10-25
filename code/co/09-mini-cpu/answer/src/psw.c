#include "psw.h"

static int sign16(uint16_t value) {
    return (value >> 15) & 1;
}

/* 同号相加、结果变号才是溢出。只看三个符号位，和 2022 年的逻辑式一致。 */
int of_add_bits(int a15, int b15, int f15) {
    return (a15 && b15 && !f15) || (!a15 && !b15 && f15);
}

/* 减法看成加上减数的补码之后，符号关系与加法不同。 */
int of_sub_bits(int a15, int b15, int f15) {
    return (a15 && !b15 && !f15) || (!a15 && b15 && f15);
}

void psw_add(uint16_t x, uint16_t y, uint16_t *sum, Psw *psw) {
    uint32_t wide = (uint32_t)x + y;
    *sum = (uint16_t)wide;
    psw->zf = *sum == 0;
    psw->sf = sign16(*sum);
    psw->cf = (wide >> 16) & 1;
    psw->of = of_add_bits(sign16(x), sign16(y), psw->sf);
}

uint16_t sub_bits(uint16_t x, uint16_t y, Psw *psw) {
    uint32_t wide = (uint32_t)x + (uint16_t)(~y) + 1u;
    uint16_t sum = (uint16_t)wide;
    /* 一次补码加法的进位取反，才是整次减法的借位。分两次加会把进位算丢。 */
    psw->zf = sum == 0;
    psw->sf = sign16(sum);
    psw->cf = ((wide >> 16) & 1) ^ 1;
    psw->of = of_sub_bits(sign16(x), sign16(y), psw->sf);
    return sum;
}
