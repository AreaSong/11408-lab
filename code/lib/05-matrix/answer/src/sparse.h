#ifndef SPARSE_H
#define SPARSE_H

#include <stdbool.h>

/* 稀疏矩阵的三元组顺序表。王道约定：行、列从 1 开始，data[0] 空着，
 * data[1..nnz] 按行优先排好。除每个非零元的 (row, col, val) 外，
 * 还必须留下矩阵的行数和列数（2023 年第 3 题）：没出现在表里的位置是 0，
 * 越界则要靠行数、列数判断，不能靠「含非零元的行数」去猜。
 * 按下标取值要扫描表，随机存取在这里已经没有了。 */

#define SP_MAX 128

typedef struct {
    int row;
    int col;
    int val;
} Triple;

typedef struct {
    int rows;
    int cols;
    int nnz;
    Triple data[SP_MAX + 1];
} Sparse;

bool sparse_init(Sparse *M, int rows, int cols);

/* 只能按行优先往表尾追加非零元。零元、越界、乱序都拒绝，表保持有序。 */
bool sparse_set(Sparse *M, int row, int col, int val);

/* 界内找不到就 *val=0，返回 true。越界返回 false。*scans 是看过的三元组个数。 */
bool sparse_get(const Sparse *M, int row, int col, int *val, int *scans);

/* 按列号一遍遍扫描。三元组按行排，同一列并不挨在一起，所以每列都要看完整张表。 */
bool sparse_transpose_slow(const Sparse *A, Sparse *B, int *visits);

/* num[1..cols]：每一列有几个非零元。调用方准备 cols+1 个 int。 */
bool sparse_count_cols(const Sparse *A, int *num);

/* cpot[1]=1，cpot[c] = cpot[c-1] + num[c-1]。这是该列第一个非零元在新表里的下标。 */
bool sparse_cpot(int cols, const int *num, int *cpot);

/* 用 num、cpot 把每个三元组一次放到转置后的位置。时间是 O(nnz + cols)。 */
bool sparse_transpose_fast(const Sparse *A, Sparse *B, int *visits);

#endif
