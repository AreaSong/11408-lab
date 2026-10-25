/*
 * 中缀转后缀，eval 时再求值。表达式不带空格，操作数是单个字母或数字。
 */
#include <stdio.h>
#include <string.h>

#include "expr.h"
#include "prompt.h"

int main(void) {
    char mode[16];
    char infix[EXPR_MAX];
    for (;;) {
        ExprInfo info;
        int got = 0;
        int do_eval = 0;
        prompt("请输入 post 或 eval，以及中缀表达式（Ctrl+D 结束）：");
        got = scanf("%15s", mode);
        if (got == EOF) {
            return 0;
        }
        if (got != 1) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        if (strcmp(mode, "post") != 0 && strcmp(mode, "eval") != 0) {
            printf("fail unknown %s\n", mode);
            continue;
        }
        if (scanf("%255s", infix) != 1) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        do_eval = strcmp(mode, "eval") == 0;
        printf("case %s expr=%s\n", mode, infix);
        expr_run(infix, do_eval, '\0', stdout, &info);
    }
}
