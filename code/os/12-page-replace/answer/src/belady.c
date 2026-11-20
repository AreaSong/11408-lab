#include "belady.h"

#include "sim.h"

#include <string.h>

typedef struct {
    int q[8];
    int n;
    int faults;
} FifoQ;

static int q_has(const FifoQ *s, int page) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (s->q[i] == page) {
            return 1;
        }
    }
    return 0;
}

static void fifo_push(FifoQ *s, int page, int cap) {
    int i;
    if (q_has(s, page)) {
        return;
    }
    s->faults++;
    if (s->n < cap) {
        s->q[s->n++] = page;
        return;
    }
    for (i = 1; i < s->n; i++) {
        s->q[i - 1] = s->q[i];
    }
    s->q[s->n - 1] = page;
}

static int walk(int *seq, int len, int target, FifoQ lo, FifoQ hi) {
    int page;
    if (len == target) {
        return lo.faults < hi.faults;
    }
    for (page = 1; page <= 5; page++) {
        FifoQ a = lo;
        FifoQ b = hi;
        seq[len] = page;
        fifo_push(&a, page, 3);
        fifo_push(&b, page, 4);
        if (walk(seq, len + 1, target, a, b)) {
            return 1;
        }
    }
    return 0;
}

static int count_pair(const BeladyFound *found, int id, int *lo, int *hi) {
    Access refs[BELADY_LEN];
    Report report;
    int i;
    int frames;

    for (i = 0; i < found->len; i++) {
        refs[i].page = found->refs[i];
        refs[i].write = 0;
    }
    for (frames = 3; frames <= 4; frames++) {
        int rc = sim_exec(algo_name(id), frames, NULL, 0, refs, found->len, &report);
        if (rc != 0) {
            return 0;
        }
        if (frames == 3) {
            *lo = report.faults;
        } else {
            *hi = report.faults;
        }
    }
    return 1;
}

int belady_search(BeladyFound *out) {
    int seq[BELADY_LEN];
    int len;
    FifoQ blank = {{0}, 0, 0};
    int id;

    memset(out, 0, sizeof *out);
    for (len = 8; len <= BELADY_LEN; len++) {
        if (!walk(seq, 0, len, blank, blank)) {
            continue;
        }
        out->len = len;
        memcpy(out->refs, seq, (size_t)len * sizeof(int));
        for (id = 0; id < 5; id++) {
            if (!count_pair(out, id, &out->faults[id][0], &out->faults[id][1])) {
                return -1;
            }
        }
        return 0;
    }
    return -1;
}
