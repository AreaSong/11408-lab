#ifndef BALANCE_H
#define BALANCE_H

/* k 路平衡归并：趟数、按内存切出的初始段数、虚段个数、选最小值的比较次数上界。 */

#define BAL_RUN_MAX 1000000000LL
#define BAL_WAY_MAX 100
#define BAL_REC_MAX 1000000000LL

typedef struct {
    int path;              /* ⌈log2(k)⌉，败者树选出一个最小值的比较次数上界 */
    long long formula;     /* 记录数 × path，选择题常用的败者树比较次数 */
    long long naive_rest;  /* (记录数 − 1) × (k − 1)，不用败者树的一种计法 */
    long long naive_all;   /* 记录数 × (k − 1)，每次都在 k 个段首里重扫 */
} SelectBound;

int merge_passes(long long runs, int ways);
int dummy_count(long long runs, int ways);
long long plain_runs(long long records, long long memory);
long long balance_io(long long records, int passes);
int select_bounds(long long records, int ways, SelectBound *out);

#endif
