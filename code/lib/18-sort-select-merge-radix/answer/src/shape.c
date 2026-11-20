#include "shape.h"

#include <stdio.h>

static int max_of(const Seq *s, int lo, int hi) {
    int m = s->a[lo].key;
    int i;
    for (i = lo + 1; i <= hi; i++) {
        if (s->a[i].key > m) {
            m = s->a[i].key;
        }
    }
    return m;
}

static int min_of(const Seq *s, int lo, int hi) {
    int m = s->a[lo].key;
    int i;
    for (i = lo + 1; i <= hi; i++) {
        if (s->a[i].key < m) {
            m = s->a[i].key;
        }
    }
    return m;
}

/* 前 k 个是最小的 k 个，并且已经非降。起泡前移和简单选择把最小换到前面，外形相同。 */
static int low_end(const Seq *s, int k) {
    if (k <= 0 || k > s->n) {
        return 0;
    }
    if (!seq_ordered(s, 0, k - 1)) {
        return 0;
    }
    if (k == s->n) {
        return 1;
    }
    return max_of(s, 0, k - 1) <= min_of(s, k, s->n - 1);
}

/* 后 k 个是最大的 k 个，并且已经非降。起泡后移和选择最大换到后面，外形相同。 */
static int high_end(const Seq *s, int k) {
    int start = s->n - k;
    if (k <= 0 || k > s->n) {
        return 0;
    }
    if (!seq_ordered(s, start, s->n - 1)) {
        return 0;
    }
    if (start == 0) {
        return 1;
    }
    return max_of(s, 0, start - 1) <= min_of(s, start, s->n - 1);
}

/* 第 pass 趟直接插入之后，有序前缀长度是 pass+1，后面的关键字可以仍是原样。 */
static int insert_ok(const Seq *s, int pass) {
    int len = pass + 1;
    if (len > s->n) {
        len = s->n;
    }
    if (len <= 1) {
        return 1;
    }
    return seq_ordered(s, 0, len - 1);
}

/* 第 pass 趟自底向上归并之后，每段长度是 2^pass，最后一段可以更短。 */
static int merge_ok(const Seq *s, int pass) {
    int block = 1;
    int p;
    int i;
    for (p = 0; p < pass; p++) {
        if (block >= s->n) {
            break;
        }
        block *= 2;
    }
    for (i = 0; i < s->n; i += block) {
        int hi = i + block - 1;
        if (hi >= s->n) {
            hi = s->n - 1;
        }
        if (!seq_ordered(s, i, hi)) {
            return 0;
        }
    }
    return 1;
}

char shape_letter(const Seq *s, int pass) {
    int bubble = low_end(s, pass) || high_end(s, pass);
    int ins = insert_ok(s, pass);
    int mer = merge_ok(s, pass);
    if (ins && !bubble && !mer) {
        return 'B';
    }
    return '?';
}

static const char *yn(int yes) {
    return yes ? "possible" : "no";
}

void shape_report(const Seq *s, int pass) {
    int low = low_end(s, pass);
    int high = high_end(s, pass);
    char pick = shape_letter(s, pass);
    printf("bubble=%s\n", yn(low || high));
    printf("select=%s\n", yn(low || high));
    printf("insert=%s\n", yn(insert_ok(s, pass)));
    printf("merge=%s\n", yn(merge_ok(s, pass)));
    printf("letter=%s\n", pick == '?' ? "none" : "B");
}
