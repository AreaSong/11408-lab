#ifndef WPL_H
#define WPL_H

#include <stdio.h>

/* 2014-41。路径长度是根到该叶的边数，根是 0。
 * 王道把根的层次记成 1，那是深度，不是这道题要乘的长度。
 * 只累加叶子。非叶的 weight 题面没有定义，打出来但不加进和。
 * argv 是权值和 '#'。0 成功，-1 词法，-2 串不够，-3 结点太多，-4 还有多余词。 */
int wpl_eval(int argc, char **argv, int *sum, FILE *trace);

#endif
