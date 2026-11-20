#include "balance.h"

#include <limits.h>

/* 封面 cover 每次乘路数，第一次 cover ≥ 段数时，乘的次数就是 ⌈log_k r⌉。 */
int merge_passes(long long runs, int ways) {
    long long cover = 1;
    int pass = 0;
    if (runs < 1 || runs > BAL_RUN_MAX || ways < 2 || ways > BAL_WAY_MAX) {
        return -1;
    }
    while (cover < runs) {
        if (cover > LLONG_MAX / ways) {
            return -1;
        }
        cover *= ways;
        pass++;
    }
    return pass;
}

/*
 * 严格 k 叉树只有度为 0 和度为 k 的结点，所以 (叶子数 − 1) 必须能被 (k − 1) 整除。
 * 余数 u = 0 时已经够了；u ≠ 0 时再补 (k − 1) − u 个权值 0 的虚段。
 * k = 2 时除数是 1，任何段数都整除，二路归并不用虚段。
 */
int dummy_count(long long runs, int ways) {
    long long rest;
    if (ways < 2 || ways > BAL_WAY_MAX || runs < 1 || runs > BAL_RUN_MAX) {
        return -1;
    }
    rest = (runs - 1) % (ways - 1);
    if (rest == 0) {
        return 0;
    }
    return (int)(ways - rest - 1);
}

/* 内存一次装 memory 个记录、在内存里排好再写回，一段最长就是 memory。 */
long long plain_runs(long long records, long long memory) {
    if (records < 1 || records > BAL_REC_MAX || memory < 1 || memory > BAL_REC_MAX) {
        return -1;
    }
    return (records + memory - 1) / memory;
}

/* 王道估算：每一趟全体记录读一遍、写一遍。段长不等时改用最佳归并树的 2×WPL。 */
long long balance_io(long long records, int passes) {
    if (records < 0 || records > BAL_REC_MAX || passes < 0) {
        return -1;
    }
    return records * 2 * (long long)passes;
}

static int ceil_log2(int ways) {
    int path = 0;
    int cover = 1;
    while (cover < ways) {
        if (cover > INT_MAX / 2) {
            return -1;
        }
        cover *= 2;
        path++;
    }
    return path;
}

int select_bounds(long long records, int ways, SelectBound *out) {
    int path = ceil_log2(ways);
    if (records < 1 || records > BAL_REC_MAX || ways < 2 || ways > BAL_WAY_MAX || path < 0) {
        return -1;
    }
    out->path = path;
    out->formula = records * path;
    out->naive_rest = (records - 1) * (ways - 1);
    out->naive_all = records * (ways - 1);
    return 0;
}
