/*
 * 括号匹配。每行是栈容量和一段不带空格的表达式。
 * 栈里只放还没配对的左括号，字母和运算符跳过。
 */
#include <stdio.h>

#include "bracket.h"
#include "prompt.h"

int main(void) {
    for (;;) {
        int cap = 0;
        char expr[256];
        BracketResult result;
        int got = 0;
        prompt("请输入栈容量和表达式（Ctrl+D 结束）：");
        got = scanf("%d %255s", &cap, expr);
        if (got == EOF) {
            return 0;
        }
        if (got != 2) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        printf("case cap=%d expr=%s\n", cap, expr);
        if (!bracket_check(expr, cap, stdout, &result)) {
            printf("fail cap=%d\n", cap);
        }
    }
}
