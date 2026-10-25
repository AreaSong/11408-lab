#ifndef ALU_H
#define ALU_H

#include "adder.h"

#include <stdio.h>

typedef enum {
    ALU_ADD = 0,
    ALU_SUB,
    ALU_AND,
    ALU_OR
} AluOp;

/*
 * 加减走同一条串行进位链。减法是加上减数的补，并把最低进位置 1。
 * 与、或只出位结果：王道这一节的简单 ALU 不给它们定义 CF、OF。
 */
typedef struct {
    AluOp op;
    Word a;
    Word b;
    Word rhs;
    int cin;
    int sub;
    int has_flags;
    Ripple ripple;
    Word result;
    int zf;
    int sf;
    int cf;
    int of;
    int c_out;
    int c_into;
    Word wide;
    int sign_hi;
    int sign_lo;
    long long math_signed;
} AluResult;

bool alu_op_parse(const char *text, AluOp *op);
AluResult alu_exec(AluOp op, Word a, Word b);

int fa_print(FILE *out, int a, int b, int cin);
int alu_print(FILE *out, const AluResult *r);
int cla_print(FILE *out, Word a, Word b);
int cond_print(FILE *out, const char *name);

#endif
