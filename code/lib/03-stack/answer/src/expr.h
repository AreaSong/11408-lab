#ifndef EXPR_H
#define EXPR_H

#include <stdbool.h>
#include <stdio.h>

/* 中缀转后缀。操作数是单个字母或数字，运算符是 + - * /，只用圆括号。
 * 同级从左往右：栈顶优先级不低于当前运算符时先弹栈顶。
 * 左括号是这一层的底；右括号把前面的运算符弹进后缀，括号自己不进后缀。
 * do=drop 表示左括号出栈。watch 对应的操作数被扫到时，运算符栈还没变，
 * 从栈底到栈顶记进 watch_stack（不带空格）。2014 年扫到 f 时应是 "+(-*"。
 * 求值只接受数字，整数除法向零截断。先弹出的是右操作数。 */
#define EXPR_MAX 256
#define EXPR_WATCH 64

typedef struct {
    char postfix[EXPR_MAX];
    char watch_stack[EXPR_WATCH];
    int max_depth;
    long long value;
    bool has_value;
    bool ok;
    const char *reason;
} ExprInfo;

/* do_eval 非 0 时接着对后缀求值。out 可以为 NULL。失败时 reason 说明原因。 */
bool expr_run(const char *infix, int do_eval, char watch, FILE *out, ExprInfo *info);

#endif
