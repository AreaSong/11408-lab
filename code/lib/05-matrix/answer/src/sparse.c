#include "sparse.h"

#include <stddef.h>

bool sparse_init(Sparse *M, int rows, int cols) {
    if (M == NULL || rows < 1 || cols < 1 || rows > SP_MAX || cols > SP_MAX) {
        return false;
    }
    M->rows = rows;
    M->cols = cols;
    M->nnz = 0;
    return true;
}

bool sparse_set(Sparse *M, int row, int col, int val) {
    Triple *last;
    if (M == NULL || val == 0 || row < 1 || col < 1 || row > M->rows || col > M->cols) {
        return false;
    }
    if (M->nnz >= SP_MAX) {
        return false;
    }
    if (M->nnz > 0) {
        last = &M->data[M->nnz];
        /* 表必须保持行优先。相同位置不能再插一次，否则转置时同一格会出现两次。 */
        if (row < last->row || (row == last->row && col <= last->col)) {
            return false;
        }
    }
    M->nnz++;
    M->data[M->nnz].row = row;
    M->data[M->nnz].col = col;
    M->data[M->nnz].val = val;
    return true;
}

bool sparse_get(const Sparse *M, int row, int col, int *val, int *scans) {
    int p;
    int seen = 0;
    if (M == NULL || val == NULL || row < 1 || col < 1 || row > M->rows || col > M->cols) {
        return false;
    }
    for (p = 1; p <= M->nnz; p++) {
        seen++;
        if (M->data[p].row == row && M->data[p].col == col) {
            *val = M->data[p].val;
            if (scans != NULL) {
                *scans = seen;
            }
            return true;
        }
        /* 后面的行号、列号只会更大，可以停。最坏仍要看完所有非零元，不是 O(1)。 */
        if (M->data[p].row > row || (M->data[p].row == row && M->data[p].col > col)) {
            break;
        }
    }
    *val = 0;
    if (scans != NULL) {
        *scans = seen;
    }
    return true;
}

static bool copy_shape(const Sparse *A, Sparse *B) {
    return A != NULL && B != NULL && A != B && sparse_init(B, A->cols, A->rows);
}

bool sparse_transpose_slow(const Sparse *A, Sparse *B, int *visits) {
    int col, p, q;
    int seen = 0;
    if (!copy_shape(A, B)) {
        return false;
    }
    q = 1;
    for (col = 1; col <= A->cols; col++) {
        for (p = 1; p <= A->nnz; p++) {
            seen++;
            if (A->data[p].col != col) {
                continue;
            }
            B->data[q].row = A->data[p].col;
            B->data[q].col = A->data[p].row;
            B->data[q].val = A->data[p].val;
            q++;
        }
    }
    B->nnz = q - 1;
    if (visits != NULL) {
        *visits = seen;
    }
    return B->nnz == A->nnz;
}

bool sparse_count_cols(const Sparse *A, int *num) {
    int col, p;
    if (A == NULL || num == NULL) {
        return false;
    }
    for (col = 1; col <= A->cols; col++) {
        num[col] = 0;
    }
    for (p = 1; p <= A->nnz; p++) {
        col = A->data[p].col;
        if (col < 1 || col > A->cols) {
            return false;
        }
        num[col]++;
    }
    return true;
}

bool sparse_cpot(int cols, const int *num, int *cpot) {
    int col;
    if (num == NULL || cpot == NULL || cols < 1 || cols > SP_MAX) {
        return false;
    }
    cpot[1] = 1;
    for (col = 2; col <= cols; col++) {
        cpot[col] = cpot[col - 1] + num[col - 1];
    }
    return true;
}

bool sparse_transpose_fast(const Sparse *A, Sparse *B, int *visits) {
    int num[SP_MAX + 1];
    int cpot[SP_MAX + 1];
    int p;
    int seen;
    if (!copy_shape(A, B) || !sparse_count_cols(A, num) || !sparse_cpot(A->cols, num, cpot)) {
        return false;
    }
    /* 数 num 看过 nnz 个三元组，求 cpot 再做 cols-1 次加法，安放时再看 nnz 个。 */
    seen = A->nnz + (A->cols > 1 ? A->cols - 1 : 0);
    for (p = 1; p <= A->nnz; p++) {
        int q = cpot[A->data[p].col];
        seen++;
        B->data[q].row = A->data[p].col;
        B->data[q].col = A->data[p].row;
        B->data[q].val = A->data[p].val;
        cpot[A->data[p].col] = q + 1;
    }
    B->nnz = A->nnz;
    if (visits != NULL) {
        *visits = seen;
    }
    return true;
}
