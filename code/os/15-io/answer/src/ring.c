#include "ring.h"

#include <stdio.h>
#include <string.h>

#define SAY(r, ...)                 \
    do {                            \
        if (!(r)->quiet) {          \
            printf(__VA_ARGS__);    \
        }                           \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;

    while (src[i] != '\0' && i < RING_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void ring_init(Ring *r) {
    int quiet = r->quiet;

    memset(r, 0, sizeof *r);
    r->quiet = quiet;
}

int ring_new(Ring *r, int cap) {
    ring_init(r);
    r->ready = 1;
    r->cap = cap;
    SAY(r, "ring cap=%d\n", cap);
    return 0;
}

int ring_put(Ring *r, const char *data) {
    int tail;

    if (r->n >= r->cap) {
        SAY(r, "result=full\n");
        return 0;
    }
    tail = (r->head + r->n) % r->cap;
    ncopy(r->data[tail], data);
    r->n++;
    SAY(r, "put data=%s n=%d\n", data, r->n);
    return 0;
}

int ring_get(Ring *r) {
    if (r->n <= 0) {
        SAY(r, "result=empty\n");
        return 0;
    }
    r->n--;
    SAY(r, "get data=%s n=%d\n", r->data[r->head], r->n);
    r->head = (r->head + 1) % r->cap;
    return 0;
}

void ring_show(const Ring *r) {
    int i;

    printf("show n=%d cap=%d order=", r->n, r->cap);
    if (r->n <= 0) {
        printf("-\n");
        return;
    }
    for (i = 0; i < r->n; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s", r->data[(r->head + i) % r->cap]);
    }
    printf("\n");
}
