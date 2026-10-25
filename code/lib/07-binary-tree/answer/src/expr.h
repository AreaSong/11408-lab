#ifndef EXPR_H
#define EXPR_H

#include <stdio.h>

/* 2017-41。中序序列加上括号。评分标准里根的 deep 从 1 起，deep>1 才加括号。
 * 叶子不加括号。单目减号的左子树为空，左递归自然不输出字符。
 * 0 成功并把中缀写进 buf。-1 词太长，-2 串不够，-3 结果装不下，-4 还有多余词。 */
int expr_eval(int argc, char **argv, char *buf, int cap, FILE *trace);

#endif
