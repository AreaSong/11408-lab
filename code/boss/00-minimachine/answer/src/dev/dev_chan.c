#include "dev/dev_chan.h"

#include <string.h>

static int in_range(int at, int n, int cap) {
    return at >= 0 && n >= 0 && at <= cap && n <= cap - at;
}

static int copy_bytes(unsigned char *dst, int dst_at, const unsigned char *src, int src_at, int n) {
    int i;
    for (i = 0; i < n; i++) {
        dst[dst_at + i] = src[src_at + i];
    }
    return 0;
}

int dev_chan_run(unsigned char *mem, int mem_n, unsigned char *dev, int dev_n, const DevChanInsn *prog,
                 int nprog, int *steps, int *irq) {
    int i;
    if (mem == NULL || dev == NULL || prog == NULL || steps == NULL || irq == NULL || nprog < 1) {
        return -1;
    }
    *steps = 0;
    *irq = 0;
    for (i = 0; i < nprog; i++) {
        const DevChanInsn *in = &prog[i];
        (*steps)++;
        if (in->op == DEV_CHAN_HALT) {
            *irq = 1;
            return 0;
        }
        if (in->op != DEV_CHAN_READ && in->op != DEV_CHAN_WRITE) {
            return -1;
        }
        if (!in_range(in->mem_at, in->n, mem_n) || !in_range(in->dev_at, in->n, dev_n)) {
            return -1;
        }
        if (in->op == DEV_CHAN_READ) {
            copy_bytes(mem, in->mem_at, dev, in->dev_at, in->n);
        } else {
            copy_bytes(dev, in->dev_at, mem, in->mem_at, in->n);
        }
    }
    return -1;
}
