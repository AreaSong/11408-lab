#include "fetch.h"

#include <string.h>

/* 请求调页只装缺的那一页。预调页再尝试装后面的相邻页，但只在还有空页框时装，不为预调去淘汰。
 * 预测错会占住页框，缺页次数可能比请求调页更多。 */

typedef struct {
    int used;
    int page;
    unsigned seq;
} FetchSlot;

static int find_page(const FetchSlot *mem, int frames, int page) {
    int i;

    for (i = 0; i < frames; i++) {
        if (mem[i].used && mem[i].page == page) {
            return i;
        }
    }
    return -1;
}

static int empty_slot(const FetchSlot *mem, int frames) {
    int i;

    for (i = 0; i < frames; i++) {
        if (!mem[i].used) {
            return i;
        }
    }
    return -1;
}

static int oldest_slot(const FetchSlot *mem, int frames) {
    int i;
    int best = -1;
    unsigned seq = 0;

    for (i = 0; i < frames; i++) {
        if (!mem[i].used) {
            continue;
        }
        if (best < 0 || mem[i].seq < seq) {
            best = i;
            seq = mem[i].seq;
        }
    }
    return best;
}

static void put_page(FetchSlot *mem, int slot, int page, unsigned *tick) {
    mem[slot].used = 1;
    mem[slot].page = page;
    mem[slot].seq = ++(*tick);
}

static int load_demand(FetchSlot *mem, int frames, int page, unsigned *tick) {
    int slot = empty_slot(mem, frames);

    if (slot < 0) {
        slot = oldest_slot(mem, frames);
        if (slot < 0) {
            return -1;
        }
    }
    put_page(mem, slot, page, tick);
    return 0;
}

static void prefetch(FetchSlot *mem, int frames, int ahead, int space, int page, unsigned *tick,
                     int *prefetched) {
    int extra = 0;
    int next = page + 1;

    while (extra < ahead && next < space) {
        int slot;
        if (find_page(mem, frames, next) >= 0) {
            next++;
            continue;
        }
        slot = empty_slot(mem, frames);
        if (slot < 0) {
            break;
        }
        put_page(mem, slot, next, tick);
        (*prefetched)++;
        extra++;
        next++;
    }
}

int fetch_run(int frames, int ahead, int space, const int *refs, int nref, int *faults, int *prefetched) {
    FetchSlot mem[FETCH_FRAMES];
    unsigned tick = 0;
    int i;

    if (frames < 1 || frames > FETCH_FRAMES || ahead < 0 || space < 1 || nref < 1) {
        return -1;
    }
    memset(mem, 0, sizeof mem);
    *faults = 0;
    *prefetched = 0;
    for (i = 0; i < nref; i++) {
        if (refs[i] < 0 || refs[i] >= space) {
            return -1;
        }
        if (find_page(mem, frames, refs[i]) >= 0) {
            continue;
        }
        (*faults)++;
        if (load_demand(mem, frames, refs[i], &tick) != 0) {
            return -1;
        }
        prefetch(mem, frames, ahead, space, refs[i], &tick, prefetched);
    }
    return 0;
}
