#ifndef ADDR_H
#define ADDR_H

#include <stdbool.h>

/* 多维数组的顺序存储。考纲：数组是随机存取结构，元素地址是下标的线性函数。
 * 行优先：最右边的下标变化最快（C 语言）。列优先：最左边的下标变化最快。
 * 下标范围由调用方给出，公式里用的是「下标 − 下界」，从 0 还是从 1 开始都走同一段代码。 */

typedef enum {
    ADDR_ROW = 0,
    ADDR_COL = 1
} AddrMajor;

typedef struct {
    int lo;
    int hi;
} AddrBound;

/* 二维：passed 是跨过的整行（行优先）或整列（列优先），each 是一行或一列的长度。 */
typedef struct {
    int passed;
    int each;
    int inner;
    long long offset;
} Addr2;

/* 三维：passed 个最慢变化的分组，每组 face 个元素；组内再跨过 mid 个 line。 */
typedef struct {
    int passed;
    int face;
    int mid;
    int line;
    int inner;
    long long offset;
} Addr3;

int addr_len(AddrBound bound);

bool addr_calc2(AddrBound row, AddrBound col, int i, int j, AddrMajor major, Addr2 *out);

bool addr_calc3(AddrBound d0, AddrBound d1, AddrBound d2, int i, int j, int k, AddrMajor major,
                Addr3 *out);

long long addr_loc(long long base, long long offset, int elem);

/* 行优先、下标从 0。用两个已知地址消掉首地址，反推列数。
 * (a2-a1)/elem = (i2-i1)*cols + (j2-j1)。除不尽或推不出正的列数就失败。 */
bool addr_infer_cols(int i1, int j1, long long a1, int i2, int j2, long long a2, int elem, int *cols);

/* 按存储次序走一遍，公式偏移必须正好是 0,1,2,…。太大（超过 20 万个元素）返回 -1。 */
int addr_scan2(AddrBound row, AddrBound col, AddrMajor major);

int addr_scan3(AddrBound d0, AddrBound d1, AddrBound d2, AddrMajor major);

#endif
