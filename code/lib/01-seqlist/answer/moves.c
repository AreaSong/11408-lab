/*
 * 实测顺序表插入、删除的平均移动次数。
 * 输入：每行一个表长 n（1 ≤ n ≤ 2000）
 * 输出：实测总次数、位置种数，以及平均次数和公式 n/2、(n-1)/2
 *
 * 插入位置有 n+1 个（含表尾），删除位置有 n 个，每种位置各做一次再撤销。
 * 只数元素后移/前移，realloc 复制不算。
 */
#include <stdio.h>

#include "prompt.h"
#include "seqlist.h"

static int build(SeqList *L, int n) {
    if (!seqlist_init(L)) {
        return 0;
    }
    for (int i = 1; i <= n; i++) {
        if (!seqlist_insert(L, i, i, NULL)) {
            return 0;
        }
    }
    return 1;
}

static long long count_inserts(SeqList *L, int n) {
    long long total = 0;
    for (int i = 1; i <= n + 1; i++) {
        int moves = 0;
        if (!seqlist_insert(L, i, -1, &moves)) {
            return -1;
        }
        total += moves;
        if (!seqlist_delete(L, i, NULL, NULL)) {
            return -1;
        }
    }
    return total;
}

static long long count_deletes(SeqList *L, int n) {
    long long total = 0;
    for (int i = 1; i <= n; i++) {
        int moves = 0;
        int value = 0;
        if (!seqlist_delete(L, i, &value, &moves)) {
            return -1;
        }
        total += moves;
        if (!seqlist_insert(L, i, value, NULL)) {
            return -1;
        }
    }
    return total;
}

static int unchanged(const SeqList *L, int n) {
    if (L->length != n) {
        return 0;
    }
    for (int i = 1; i <= n; i++) {
        int value = 0;
        if (!seqlist_get(L, i, &value) || value != i) {
            return 0;
        }
    }
    return 1;
}

static int report(int n) {
    SeqList list;
    if (!build(&list, n)) {
        seqlist_destroy(&list);
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    long long insert_total = count_inserts(&list, n);
    long long delete_total = count_deletes(&list, n);
    int intact = unchanged(&list, n);
    seqlist_destroy(&list);
    if (insert_total < 0 || delete_total < 0 || !intact) {
        fprintf(stderr, "统计过程中顺序表被破坏\n");
        return 0;
    }
    long long insert_formula = (long long)n * (n + 1) / 2;
    long long delete_formula = (long long)n * (n - 1) / 2;
    printf("n=%d insert total=%lld trials=%d avg=%.1f expect=%.1f "
           "delete total=%lld trials=%d avg=%.1f expect=%.1f\n",
           n, insert_total, n + 1, (double)insert_total / (n + 1), n / 2.0, delete_total, n,
           (double)delete_total / n, (n - 1) / 2.0);
    if (insert_total != insert_formula || delete_total != delete_formula) {
        fprintf(stderr, "平均移动次数与公式不一致\n");
        return 0;
    }
    return 1;
}

int main(void) {
    int n = 0;
    for (;;) {
        prompt("请输入表长 n（每行一个，1 到 2000，Ctrl+D 结束）：");
        int rc = scanf("%d", &n);
        if (rc == EOF) {
            break;
        }
        if (rc != 1 || n < 1 || n > 2000) {
            fprintf(stderr, "输入格式错误：n 应为 1 到 2000 的整数\n");
            return 1;
        }
        if (!report(n)) {
            return 1;
        }
    }
    return 0;
}
