#include "insert.h"

#include <stdio.h>

/*
 * 从右往左找第一个小于等于待插关键字的位置，插在它后面。
 * 只在严格大于时后移，相等关键字保持原次序，所以直接插入是稳定的。
 * 元素留在原位时不计移动；真正插入时，暂存、每一次后移、写回各记 1 次。
 */
void gap_insert(Seq *s, int i, int d) {
    Rec tmp;
    int j;
    int shifted = 0;
    if (i < d) {
        return;
    }
    tmp = s->a[i];
    j = i - d;
    while (j >= 0) {
        s->cmp++;
        if (s->a[j].key <= tmp.key) {
            break;
        }
        s->a[j + d] = s->a[j];
        s->move++;
        shifted = 1;
        j -= d;
    }
    if (!shifted) {
        return;
    }
    s->move++;
    s->a[j + d] = tmp;
    s->move++;
}

/* 相等时 low 继续向右，插在已有相等关键字的后面，折半插入因此也稳定。 */
static int binary_at(Seq *s, int i) {
    int low = 0;
    int high = i - 1;
    int key = s->a[i].key;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        s->cmp++;
        if (s->a[mid].key > key) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return low;
}

static void binary_place(Seq *s, int i) {
    Rec tmp;
    int low = binary_at(s, i);
    int j;
    if (low == i) {
        return;
    }
    tmp = s->a[i];
    s->move++;
    for (j = i - 1; j >= low; j--) {
        s->a[j + 1] = s->a[j];
        s->move++;
    }
    s->a[low] = tmp;
    s->move++;
}

static int insert_run(Seq *s, int binary, Seq *out, int cap, int printing) {
    int i;
    int npass = 0;
    if (s->n <= 1) {
        if (printing) {
            printf("sorted\n");
            seq_print(s);
            seq_stats(s);
        }
        return 0;
    }
    for (i = 1; i < s->n; i++) {
        if (binary) {
            binary_place(s, i);
        } else {
            gap_insert(s, i, 1);
        }
        npass++;
        if (printing) {
            /* 第 i 趟把原下标 i 插进前面，有序前缀长度是 i+1。 */
            printf("pass %d head=%d\n", i, i + 1);
            seq_print(s);
        }
        if (out != NULL && npass <= cap) {
            out[npass - 1] = *s;
        }
    }
    if (printing) {
        seq_stats(s);
    }
    return npass;
}

int insert_trace(Seq *s, int binary) {
    return insert_run(s, binary, NULL, 0, 1);
}

int insert_collect(const Seq *src, Seq *out, int cap, int binary) {
    Seq s = *src;
    return insert_run(&s, binary, out, cap, 0);
}
