#ifndef SPECIAL_H
#define SPECIAL_H

#include <stdbool.h>

/* 特殊矩阵的压缩存储。约定与王道、408 真题一致：
 * 矩阵元素下标 i、j 从 1 开始，压缩数组下标 k 从 0 开始（C 语言数组）。
 * 题目若把一维数组写成从 1 开始，把这里的 k 加 1。
 *
 * 对称矩阵只存一半，另一半交换下标后落到同一格，仍是 O(1)。
 * 三角矩阵的另一半全是同一个常数，所有这种位置共用最后一格。
 * 三对角只存 |i-j|<=1，其余位置没有存储单元。 */

#define MAP_N_MAX 500
#define MAP_VERIFY_MAX 64
/* 64 阶下三角（含对角线）恰好 64*65/2 个。 */
#define MAP_ORDER_MAX 2080

typedef enum {
    MAP_SYM_LOWER = 0,
    MAP_SYM_UPPER_ROW,
    MAP_SYM_UPPER_COL,
    MAP_TRI_LOWER,
    MAP_TRI_UPPER,
    MAP_BAND
} MapKind;

typedef struct {
    int i;
    int j;
} MapPos;

typedef struct {
    int swapped;
    int si;
    int sj;
    int constant;
    int prefix;
    int inner;
    int k;
} MapExplain;

bool map_explain(MapKind kind, int n, int i, int j, MapExplain *out);

/* 压缩后占用的单元数。三角矩阵比对称矩阵多 1，用来放常数。 */
int map_stored(MapKind kind, int n);

/* 按实际存储次序枚举被存下来的元素（不含三角矩阵末尾那个常数）。n 不超过 MAP_VERIFY_MAX。 */
int map_order(MapKind kind, int n, MapPos *out, int cap);

/* 三对角逆映射。i = (k+1)/3 + 1，j = k - 2*i + 3。k 从 0 开始。 */
bool map_band_inv(int n, int k, int *i, int *j);

/* 存储次序上的序号必须等于公式算出的 k；每个位置再查回去，镜像和常数也要落到对的格子。 */
bool map_verify(MapKind kind, int n);

#endif
