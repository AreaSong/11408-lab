#ifndef POPSEQ_H
#define POPSEQ_H

#include <stdbool.h>
#include <stdio.h>

/* 入栈顺序固定为 1, 2, …, n。合法出栈序列的个数是卡特兰数。
 * 判断某一个序列：想弹出 x 时，栈顶不是 x 就把下一个数压进来；
 * 数已经用完仍对不上，这个序列就不合法。
 * enum 把序列都列出来再和卡特兰数对账，n 最大 8（C_8 = 1430）。
 * count 只计数，n 最大 12。check 模拟单个序列，n 最大 64。 */
#define POP_CHECK_MAX 64
#define POP_ENUM_MAX 8
#define POP_COUNT_MAX 12

/* n 超出 0..POP_COUNT_MAX 时返回 -1。C_0 = 1。 */
long long popseq_catalan(int n);

/* 不打印。n 超出 1..POP_COUNT_MAX 时返回 -1。 */
long long popseq_count(int n);

/* 打印 want / push / pop。合法返回 true。 */
bool popseq_check(int n, const int *seq, FILE *out);

/* 按字典序列出全部合法序列。成功返回个数，n 不合法或内存不足返回 -1。 */
int popseq_enum(int n, FILE *out);

/* 只列出第一个数是 first 的合法序列，返回条数。n 不合法返回 -1。 */
int popseq_prefix(int n, int first, FILE *out);

#endif
