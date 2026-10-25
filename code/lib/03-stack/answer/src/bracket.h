#ifndef BRACKET_H
#define BRACKET_H

#include <stdbool.h>
#include <stdio.h>

/* 括号匹配。左括号入栈，右括号必须和栈顶配对后弹出。
 * 字母和运算符跳过，栈里只有还没配上的左括号。
 * cap 是栈容量：嵌套深度超过 cap 就是上溢，2025 年第 2 题考的就是这件事。
 * 容量范围 1..SQ_MAX。 */
enum {
    BR_OK = 0,
    BR_OVERFLOW = 1,
    BR_MISMATCH = 2,
    BR_UNCLOSED = 3
};

typedef struct {
    int max_depth;
    int status;
} BracketResult;

/* cap 不合法或参数为空时返回 false，并且不打印。其余情况打印过程并返回 true。 */
bool bracket_check(const char *expr, int cap, FILE *out, BracketResult *result);

#endif
