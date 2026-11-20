#include "sim_priv.h"

/*
 * OPT 淘汰「下一次用得最晚」的页。以后不再出现，比任何一次真实访问都更晚。
 * 同样远时留页框号大的，淘汰页框号小的。王道例题通常不会打成平手。
 * 它要预知访问串，不能在真机器上实现，只用来给别的算法当下限。
 */

static int next_use(const Access *refs, int n, int from, int page) {
    int i;
    for (i = from; i < n; i++) {
        if (refs[i].page == page) {
            return i;
        }
    }
    return n;
}

static int opt_victim(const int *page, int frames, const Access *refs, int n, int future) {
    int best = -1;
    int best_at = -1;
    int i;

    for (i = 0; i < frames; i++) {
        int at;
        if (page[i] < 0) {
            continue;
        }
        at = next_use(refs, n, future, page[i]);
        if (at > best_at) {
            best_at = at;
            best = i;
        }
    }
    return best;
}

static int opt_find(const int *page, int frames, int want) {
    int i;
    for (i = 0; i < frames; i++) {
        if (page[i] == want) {
            return i;
        }
    }
    return -1;
}

static int opt_empty(const int *page, int frames) {
    int i;
    for (i = 0; i < frames; i++) {
        if (page[i] < 0) {
            return i;
        }
    }
    return -1;
}

int opt_sim(int frames, const Access *warm, int nwarm, const Access *refs, int nrefs,
            Report *out) {
    int page[FRAME_MAX];
    int i;

    for (i = 0; i < frames; i++) {
        page[i] = -1;
    }
    for (i = 0; i < nwarm; i++) {
        int slot = opt_empty(page, frames);
        if (slot < 0 || opt_find(page, frames, warm[i].page) >= 0) {
            return opt_find(page, frames, warm[i].page) >= 0 ? -3 : -2;
        }
        page[slot] = warm[i].page;
    }
    for (i = 0; i < nrefs; i++) {
        Step *s = &out->step[i];
        int slot = opt_find(page, frames, refs[i].page);
        int victim = -1;

        step_blank(s, frames);
        s->page = refs[i].page;
        s->write = refs[i].write;
        if (slot >= 0) {
            s->fault = 0;
        } else {
            slot = opt_empty(page, frames);
            s->fault = 1;
            out->faults++;
            if (slot < 0) {
                slot = opt_victim(page, frames, refs, nrefs, i + 1);
                if (slot < 0) {
                    return -2;
                }
                victim = page[slot];
                s->victim = victim;
                out->replaces++;
            }
            page[slot] = refs[i].page;
        }
        for (slot = 0; slot < frames; slot++) {
            s->slots[slot] = page[slot];
        }
    }
    return 0;
}
