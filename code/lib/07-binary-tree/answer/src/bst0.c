#include "bst0.h"

#include <stddef.h>

static void collect(const int *a, int n, int i, int *seq, int *sn) {
    if (i >= n || a[i] == -1) {
        return;
    }
    collect(a, n, 2 * i + 1, seq, sn);
    if (*sn < BST0_MAX) {
        seq[(*sn)++] = a[i];
    }
    collect(a, n, 2 * i + 2, seq, sn);
}

static int rising(const int *a, int n, int i, int *prev) {
    if (i >= n || a[i] == -1) {
        return 1;
    }
    if (!rising(a, n, 2 * i + 1, prev)) {
        return 0;
    }
    /* 考纲：二叉搜索树中序严格递增。题面结点值都是正整数，所以 prev 从 0 起。 */
    if (a[i] <= *prev) {
        return 0;
    }
    *prev = a[i];
    return rising(a, n, 2 * i + 2, prev);
}

int bst0_inorder(const int *a, int n, int *seq, int *sn) {
    if (a == NULL || seq == NULL || sn == NULL || n < 1 || n > BST0_MAX) {
        return -1;
    }
    *sn = 0;
    collect(a, n, 0, seq, sn);
    return 0;
}

int bst0_is(const int *a, int n) {
    int prev = 0;
    int seq[BST0_MAX];
    int sn = 0;
    if (bst0_inorder(a, n, seq, &sn) != 0 || sn == 0) {
        return 0;
    }
    return rising(a, n, 0, &prev);
}
