/*
 * 稀疏矩阵三元组：建表、按位置取值、普通转置、快速转置。
 * data[0] 不用，下标从 1 开始，和王道的三元组表一致。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "sparse.h"

static void print_table(const Sparse *M) {
    int p;
    for (p = 1; p <= M->nnz; p++) {
        printf("[%d] %d %d %d\n", p, M->data[p].row, M->data[p].col, M->data[p].val);
    }
}

static int cmd_init(Sparse *M) {
    int rows = 0;
    int cols = 0;
    if (scanf("%d %d", &rows, &cols) != 2) {
        return 0;
    }
    if (!sparse_init(M, rows, cols)) {
        printf("fail init rows=%d cols=%d\n", rows, cols);
        return 1;
    }
    printf("init rows=%d cols=%d nnz=0\n", M->rows, M->cols);
    printf("keep=rows,cols\n");
    printf("drop=nonzero_rows,nonzero_cols\n");
    return 1;
}

static int cmd_set(Sparse *M) {
    int row = 0;
    int col = 0;
    int val = 0;
    if (scanf("%d %d %d", &row, &col, &val) != 3) {
        return 0;
    }
    if (M->rows == 0) {
        printf("fail set noinit\n");
        return 1;
    }
    if (val == 0) {
        printf("fail set zero\n");
        return 1;
    }
    if (row < 1 || col < 1 || row > M->rows || col > M->cols) {
        printf("fail set range i=%d j=%d\n", row, col);
        return 1;
    }
    if (M->nnz >= SP_MAX) {
        printf("fail set full\n");
        return 1;
    }
    if (!sparse_set(M, row, col, val)) {
        printf("fail set order i=%d j=%d\n", row, col);
        return 1;
    }
    printf("set i=%d j=%d v=%d nnz=%d\n", row, col, val, M->nnz);
    return 1;
}

static int cmd_get(const Sparse *M) {
    int row = 0;
    int col = 0;
    int val = 0;
    int scans = 0;
    if (scanf("%d %d", &row, &col) != 2) {
        return 0;
    }
    if (M->rows == 0 || !sparse_get(M, row, col, &val, &scans)) {
        printf("fail get range i=%d j=%d\n", row, col);
        return 1;
    }
    printf("get i=%d j=%d val=%d scans=%d\n", row, col, val, scans);
    return 1;
}

static int cmd_dump(const Sparse *M) {
    if (M->rows == 0) {
        printf("fail dump noinit\n");
        return 1;
    }
    printf("dump rows=%d cols=%d nnz=%d\n", M->rows, M->cols, M->nnz);
    print_table(M);
    return 1;
}

static void print_nums(const char *label, const int *a, int n) {
    int i;
    printf("%s", label);
    for (i = 1; i <= n; i++) {
        printf("%d%s", a[i], i == n ? "\n" : " ");
    }
}

static int cmd_slow(const Sparse *M) {
    Sparse out;
    int visits = 0;
    if (M->rows == 0 || !sparse_transpose_slow(M, &out, &visits)) {
        printf("fail slow\n");
        return 1;
    }
    printf("slow visits=%d\n", visits);
    print_table(&out);
    return 1;
}

static int cmd_fast(const Sparse *M) {
    Sparse out;
    int num[SP_MAX + 1];
    int cpot[SP_MAX + 1];
    int after[SP_MAX + 1];
    int col;
    int visits = 0;
    if (M->rows == 0 || !sparse_count_cols(M, num) || !sparse_cpot(M->cols, num, cpot)) {
        printf("fail fast\n");
        return 1;
    }
    for (col = 1; col <= M->cols; col++) {
        after[col] = cpot[col] + num[col];
    }
    if (!sparse_transpose_fast(M, &out, &visits)) {
        printf("fail fast\n");
        return 1;
    }
    printf("fast visits=%d\n", visits);
    print_nums("num ", num, M->cols);
    print_nums("cpot ", cpot, M->cols);
    print_nums("cpot_after ", after, M->cols);
    print_table(&out);
    return 1;
}

static int dispatch(Sparse *M, const char *cmd) {
    if (strcmp(cmd, "init") == 0) {
        return cmd_init(M);
    }
    if (strcmp(cmd, "set") == 0) {
        return cmd_set(M);
    }
    if (strcmp(cmd, "get") == 0) {
        return cmd_get(M);
    }
    if (strcmp(cmd, "dump") == 0) {
        return cmd_dump(M);
    }
    if (strcmp(cmd, "slow") == 0) {
        return cmd_slow(M);
    }
    if (strcmp(cmd, "fast") == 0) {
        return cmd_fast(M);
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    Sparse matrix;
    char cmd[16];
    matrix.rows = 0;
    matrix.cols = 0;
    matrix.nnz = 0;
    for (;;) {
        prompt("请输入命令（init / set / dump / get / slow / fast）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(&matrix, cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
