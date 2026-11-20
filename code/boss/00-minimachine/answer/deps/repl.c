#include "repl.h"

#include <string.h>

typedef struct {
    int key;
    int origin;
    int born;
    int live;
} Slot;

static int live_count(const Slot *wa, int memory) {
    int i;
    int count = 0;
    for (i = 0; i < memory; i++) {
        if (wa[i].live) {
            count++;
        }
    }
    return count;
}

static int take_empty(const Slot *wa, int memory) {
    int i;
    for (i = 0; i < memory; i++) {
        if (!wa[i].live) {
            return i;
        }
    }
    return -1;
}

/* 有门槛时只看不小于上一输出的记录。关键字相同则选更早进入工作区的，保持稳定。 */
static int pick_slot(const Slot *wa, int memory, int has_last, int last) {
    int i;
    int best = -1;
    for (i = 0; i < memory; i++) {
        if (!wa[i].live || (has_last && wa[i].key < last)) {
            continue;
        }
        if (best < 0 || wa[i].key < wa[best].key ||
            (wa[i].key == wa[best].key && wa[i].born < wa[best].born)) {
            best = i;
        }
    }
    return best;
}

static int load_one(Slot *wa, int memory, const int *in, int *src, int n, int *born) {
    int slot;
    if (*src >= n || live_count(wa, memory) >= memory) {
        return 0;
    }
    slot = take_empty(wa, memory);
    wa[slot].live = 1;
    wa[slot].key = in[*src];
    wa[slot].origin = *src;
    wa[slot].born = (*born)++;
    (*src)++;
    return 1;
}

static int emit(Slot *wa, int slot, int *has_last, int *last, Repl *out, int *produced) {
    if (!*has_last) {
        if (out->runs >= REPL_RUNS) {
            return -1;
        }
        out->len[out->runs] = 0;
        out->runs++;
    }
    *has_last = 1;
    *last = wa[slot].key;
    out->key[*produced] = wa[slot].key;
    out->ord[*produced] = wa[slot].origin;
    out->len[out->runs - 1]++;
    (*produced)++;
    wa[slot].live = 0;
    return 0;
}

int repl_select(int memory, const int *in, int n, Repl *out) {
    Slot wa[REPL_M];
    int src = 0;
    int born = 0;
    int has_last = 0;
    int last = 0;
    int produced = 0;
    int guard = 0;
    int i;
    if (memory < 1 || memory > REPL_M || n < 1 || n > REPL_N) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (in[i] < EXT_KEY_MIN || in[i] > EXT_KEY_MAX) {
            return -1;
        }
    }
    memset(out, 0, sizeof(*out));
    memset(wa, 0, sizeof(wa));
    out->n = n;
    out->bound_max = n;
    /* 题目条件是 n 远大于工作区。工作区能装下整个文件时，第一段一定会收进全部记录。 */
    out->bound_min = memory < n ? memory : n;
    while (produced < n) {
        int slot;
        if (++guard > n * 4) {
            return -1;
        }
        if (load_one(wa, memory, in, &src, n, &born)) {
            continue;
        }
        slot = pick_slot(wa, memory, has_last, last);
        if (slot < 0) {
            if (!has_last) {
                return -1;
            }
            has_last = 0;
            continue;
        }
        if (emit(wa, slot, &has_last, &last, out, &produced) != 0) {
            return -1;
        }
    }
    out->first = out->len[0];
    return 0;
}

void repl_print(const Repl *result, FILE *out) {
    int run;
    int at = 0;
    for (run = 0; run < result->runs; run++) {
        int i;
        fprintf(out, "run %d\n", run + 1);
        for (i = 0; i < result->len[run]; i++) {
            if (i > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", result->key[at++]);
        }
        fputc('\n', out);
    }
    fprintf(out, "runs=%d\n", result->runs);
    fprintf(out, "first=%d\n", result->first);
    fprintf(out, "bound_max=%d\n", result->bound_max);
    fprintf(out, "bound_min=%d\n", result->bound_min);
}
