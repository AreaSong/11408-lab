#include "dev/dev_ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RING_MAX 16
#define RING_NAME 16
#define POOL_MAX 8

struct DevRing {
    FILE *out;
    int cap;
    int n;
    int head;
    char data[RING_MAX][RING_NAME];
};

struct DevPool {
    FILE *out;
    int cap;
    int free_n;
    int free_id[POOL_MAX];
    int used[POOL_MAX];
};

#define SAY(fp, ...)                         \
    do {                                     \
        if ((fp) != NULL) {                  \
            fprintf((fp), __VA_ARGS__);      \
        }                                    \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0' && i < RING_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

DevRing *dev_ring_new(int cap, FILE *out) {
    DevRing *r;
    if (cap < 1 || cap > RING_MAX) {
        return NULL;
    }
    r = calloc(1, sizeof *r);
    if (r == NULL) {
        return NULL;
    }
    r->out = out;
    r->cap = cap;
    SAY(out, "ring cap=%d\n", cap);
    return r;
}

void dev_ring_free(DevRing *r) { free(r); }

int dev_ring_put(DevRing *r, const char *data) {
    int tail;
    if (r == NULL || data == NULL || data[0] == '\0') {
        return -1;
    }
    if (r->n >= r->cap) {
        SAY(r->out, "result=full\n");
        return 0;
    }
    tail = (r->head + r->n) % r->cap;
    ncopy(r->data[tail], data);
    r->n++;
    SAY(r->out, "put data=%s n=%d\n", data, r->n);
    return 0;
}

int dev_ring_get(DevRing *r) {
    if (r == NULL) {
        return -1;
    }
    if (r->n <= 0) {
        SAY(r->out, "result=empty\n");
        return 0;
    }
    r->n--;
    SAY(r->out, "get data=%s n=%d\n", r->data[r->head], r->n);
    r->head = (r->head + 1) % r->cap;
    return 0;
}

void dev_ring_show(const DevRing *r) {
    int i;
    if (r == NULL || r->out == NULL) {
        return;
    }
    fprintf(r->out, "show n=%d cap=%d order=", r->n, r->cap);
    if (r->n <= 0) {
        fprintf(r->out, "-\n");
        return;
    }
    for (i = 0; i < r->n; i++) {
        if (i > 0) {
            fprintf(r->out, ",");
        }
        fprintf(r->out, "%s", r->data[(r->head + i) % r->cap]);
    }
    fprintf(r->out, "\n");
}

int dev_ring_count(const DevRing *r) { return r == NULL ? -1 : r->n; }

DevPool *dev_pool_new(int cap, FILE *out) {
    DevPool *p;
    int i;
    if (cap < 1 || cap > POOL_MAX) {
        return NULL;
    }
    p = calloc(1, sizeof *p);
    if (p == NULL) {
        return NULL;
    }
    p->out = out;
    p->cap = cap;
    p->free_n = cap;
    for (i = 0; i < cap; i++) {
        p->free_id[i] = i;
    }
    SAY(out, "pool cap=%d free=%d\n", cap, cap);
    return p;
}

void dev_pool_free(DevPool *p) { free(p); }

int dev_pool_get(DevPool *p) {
    int id;
    int i;
    if (p == NULL) {
        return -1;
    }
    if (p->free_n <= 0) {
        SAY(p->out, "result=empty\n");
        return -1;
    }
    id = p->free_id[0];
    for (i = 1; i < p->free_n; i++) {
        p->free_id[i - 1] = p->free_id[i];
    }
    p->free_n--;
    p->used[id] = 1;
    SAY(p->out, "pool get id=%d free=%d\n", id, p->free_n);
    return id;
}

int dev_pool_put(DevPool *p, int id) {
    if (p == NULL || id < 0 || id >= p->cap || !p->used[id]) {
        return -1;
    }
    p->used[id] = 0;
    p->free_id[p->free_n++] = id;
    SAY(p->out, "pool put id=%d free=%d\n", id, p->free_n);
    return 0;
}

int dev_pool_free_count(const DevPool *p) { return p == NULL ? -1 : p->free_n; }
