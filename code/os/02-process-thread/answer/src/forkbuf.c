#include "forkbuf.h"

#include <string.h>

typedef struct {
    char data[8];
    int n;
} ForkBuf;

static int flush_on_print(const char *mode) {
    return strcmp(mode, "nolf") != 0;
}

/* 从「刚要执行 printf」接着走。fork 出来的子进程和父进程都从这里开始。 */
static int after_print(int i, ForkBuf buf, int flush, int *procs);

static int at_loop(int i, ForkBuf buf, int flush, int *procs) {
    if (i >= 2) {
        *procs += 1;
        return buf.n;
    }
    return after_print(i, buf, flush, procs) + after_print(i, buf, flush, procs);
}

static int after_print(int i, ForkBuf buf, int flush, int *procs) {
    int emitted = 0;
    buf.data[buf.n++] = '-';
    if (flush) {
        emitted = buf.n;
        buf.n = 0;
    }
    return emitted + at_loop(i + 1, buf, flush, procs);
}

bool forkbuf_count(const char *mode, int *chars, int *procs) {
    ForkBuf empty;
    int flush;
    if (mode == NULL || chars == NULL || procs == NULL) {
        return false;
    }
    if (strcmp(mode, "nolf") != 0 && strcmp(mode, "line") != 0 && strcmp(mode, "flush") != 0) {
        return false;
    }
    flush = flush_on_print(mode);
    memset(&empty, 0, sizeof empty);
    *procs = 0;
    *chars = at_loop(0, empty, flush, procs);
    return true;
}
