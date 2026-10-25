#ifndef ARITH_H
#define ARITH_H

/* 放在单独的翻译单元里，编译器看不到实参，必须发出真正的除法或读内存指令。 */
int divide_int(int dividend, int divisor);

int load_int(volatile int *p);

#endif
