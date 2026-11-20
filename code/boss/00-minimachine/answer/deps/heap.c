#include "heap.h"

#include <stdio.h>

void heap_init(Heap *h, int is_max) {
    h->n = 0;
    h->is_max = is_max ? 1 : 0;
}

int heap_load(Heap *h, const int *keys, int n, int is_max) {
    if (n < 0 || n > HEAP_MAX) {
        return HEAP_RANGE;
    }
    heap_init(h, is_max);
    for (int i = 0; i < n; i++) {
        h->key[i + 1] = keys[i];
    }
    h->n = n;
    return HEAP_OK;
}

void heap_fprint(const Heap *h, FILE *out) {
    if (h->n == 0) {
        fprintf(out, "-\n");
        return;
    }
    for (int i = 1; i <= h->n; i++) {
        fprintf(out, "%s%d", i == 1 ? "" : " ", h->key[i]);
    }
    fputc('\n', out);
}

/* 父结点比孩子更不该当父：大根堆里父小于孩子，小根堆里父大于孩子。相等算已经成堆。 */
static int disorder(const Heap *h, int parent, int child) {
    if (h->is_max) {
        return h->key[parent] < h->key[child];
    }
    return h->key[parent] > h->key[child];
}

int heap_ok(const Heap *h) {
    for (int i = 2; i <= h->n; i++) {
        if (disorder(h, i / 2, i)) {
            return 0;
        }
    }
    return 1;
}

static void swap_at(Heap *h, int i, int j) {
    int tmp = h->key[i];
    h->key[i] = h->key[j];
    h->key[j] = tmp;
}

/*
 * 左右孩子一样大时留在左边。王道筛选写的是
 * 「右孩子的关键字严格大于左孩子才把下标拨到右边」。
 */
static int pick_child(const Heap *h, int i) {
    int left = i * 2;
    int right = left + 1;
    if (left > h->n) {
        return 0;
    }
    if (right > h->n) {
        return left;
    }
    if (h->is_max && h->key[right] > h->key[left]) {
        return right;
    }
    if (!h->is_max && h->key[right] < h->key[left]) {
        return right;
    }
    return left;
}

static void show_now(const Heap *h, FILE *out) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "now ");
    heap_fprint(h, out);
}

static void sift_down(Heap *h, int i, FILE *out) {
    if (out != NULL) {
        fprintf(out, "sift %d\n", i);
    }
    for (;;) {
        int child = pick_child(h, i);
        if (child == 0 || !disorder(h, i, child)) {
            return;
        }
        if (out != NULL) {
            fprintf(out, "swap %d %d\n", i, child);
        }
        swap_at(h, i, child);
        show_now(h, out);
        i = child;
    }
}

static void sift_up(Heap *h, int i, FILE *out) {
    if (out != NULL) {
        fprintf(out, "sift %d\n", i);
    }
    while (i > 1 && disorder(h, i / 2, i)) {
        int parent = i / 2;
        if (out != NULL) {
            fprintf(out, "swap %d %d\n", i, parent);
        }
        swap_at(h, i, parent);
        show_now(h, out);
        i = parent;
    }
}

static void show_heap(const Heap *h, FILE *out) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "heap ");
    heap_fprint(h, out);
}

int heap_build(Heap *h, const int *keys, int n, int is_max, FILE *out) {
    int rc = heap_load(h, keys, n, is_max);
    if (rc != HEAP_OK || n < 1) {
        return n < 1 ? HEAP_RANGE : rc;
    }
    if (out != NULL) {
        fprintf(out, "build %s\nstart ", h->is_max ? "max" : "min");
        heap_fprint(h, out);
    }
    /* 最后一个非叶是 ⌊n/2⌋。叶子已经是堆，从它前面逐个向下筛。 */
    for (int i = n / 2; i >= 1; i--) {
        sift_down(h, i, out);
    }
    show_heap(h, out);
    return HEAP_OK;
}

int heap_push(Heap *h, int key, FILE *out) {
    if (h->n >= HEAP_MAX) {
        return HEAP_FULL;
    }
    h->n++;
    h->key[h->n] = key;
    if (out != NULL) {
        fprintf(out, "push %d\nplace ", key);
        heap_fprint(h, out);
    }
    sift_up(h, h->n, out);
    show_heap(h, out);
    return HEAP_OK;
}

int heap_insert_all(Heap *h, const int *keys, int n, int is_max, FILE *out) {
    if (n < 1 || n > HEAP_MAX) {
        return HEAP_RANGE;
    }
    heap_init(h, is_max);
    if (out != NULL) {
        fprintf(out, "insert %s\n", h->is_max ? "max" : "min");
    }
    for (int i = 0; i < n; i++) {
        int rc = heap_push(h, keys[i], out);
        if (rc != HEAP_OK) {
            return rc;
        }
    }
    return HEAP_OK;
}

int heap_replace_root(Heap *h, int key, FILE *out) {
    if (h->n < 1) {
        return HEAP_EMPTY;
    }
    if (out != NULL) {
        fprintf(out, "replace %d\n", key);
    }
    h->key[1] = key;
    if (out != NULL) {
        fprintf(out, "place ");
        heap_fprint(h, out);
    }
    sift_down(h, 1, out);
    show_heap(h, out);
    return HEAP_OK;
}

int heap_pop(Heap *h, int *got, FILE *out) {
    if (h->n < 1) {
        return HEAP_EMPTY;
    }
    *got = h->key[1];
    if (out != NULL) {
        fprintf(out, "pop %d\n", *got);
    }
    h->key[1] = h->key[h->n];
    h->n--;
    if (h->n == 0) {
        show_heap(h, out);
        return HEAP_OK;
    }
    if (out != NULL) {
        fprintf(out, "place ");
        heap_fprint(h, out);
    }
    sift_down(h, 1, out);
    show_heap(h, out);
    return HEAP_OK;
}
