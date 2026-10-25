#include "topk.h"

#include "heap.h"

static void sort_desc(int *a, int n) {
    for (int i = 0; i < n; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) {
            if (a[j] > a[best]) {
                best = j;
            }
        }
        int tmp = a[i];
        a[i] = a[best];
        a[best] = tmp;
    }
}

static void show_largest(const Heap *h, FILE *out) {
    int copy[HEAP_MAX];
    for (int i = 0; i < h->n; i++) {
        copy[i] = h->key[i + 1];
    }
    sort_desc(copy, h->n);
    fprintf(out, "largest");
    for (int i = 0; i < h->n; i++) {
        fprintf(out, " %d", copy[i]);
    }
    fputc('\n', out);
}

int topk_run(const int *keys, int n, int k, FILE *out) {
    Heap h;
    if (n < 1 || n > HEAP_MAX || k < 1 || k > HEAP_MAX) {
        return HEAP_RANGE;
    }
    heap_init(&h, 0);
    fprintf(out, "topk k=%d heap=min\n", k);
    for (int i = 0; i < n; i++) {
        if (h.n < k) {
            heap_push(&h, keys[i], out);
            continue;
        }
        if (keys[i] <= h.key[1]) {
            fprintf(out, "skip %d\n", keys[i]);
            continue;
        }
        heap_replace_root(&h, keys[i], out);
    }
    show_largest(&h, out);
    return HEAP_OK;
}
