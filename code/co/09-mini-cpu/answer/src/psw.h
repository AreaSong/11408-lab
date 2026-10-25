#ifndef PSW_H
#define PSW_H

#include <stdint.h>

/* PSW 里和运算有关的四位。ZF/SF 看结果，CF 看无符号进位，OF 看有符号溢出。 */
typedef struct {
    int zf;
    int sf;
    int cf;
    int of;
} Psw;

void psw_add(uint16_t x, uint16_t y, uint16_t *sum, Psw *psw);
int of_add_bits(int a15, int b15, int f15);
int of_sub_bits(int a15, int b15, int f15);
uint16_t sub_bits(uint16_t x, uint16_t y, Psw *psw);

#endif
