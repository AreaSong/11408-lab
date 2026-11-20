#include "sim_priv.h"

#include "order.h"

#include <string.h>

static int place_empty(int *page, int cap) {
    int i;
    for (i = 0; i < cap; i++) {
        if (page[i] < 0) {
            return i;
        }
    }
    return -1;
}

static int find_page(const int *page, int cap, int want) {
    int i;
    for (i = 0; i < cap; i++) {
        if (page[i] == want) {
            return i;
        }
    }
    return -1;
}

static void snap(const Order *o, const int *page, int cap, Step *s) {
    int i;
    int n = order_len(o);

    for (i = 0; i < cap; i++) {
        s->slots[i] = page[i];
    }
    s->norder = n;
    for (i = 0; i < n; i++) {
        s->order[i] = order_at(o, i + 1);
    }
}

static int warm_up(Order *o, int *page, int cap, const Access *warm, int nwarm) {
    int i;

    for (i = 0; i < nwarm; i++) {
        int slot;
        if (order_has(o, warm[i].page)) {
            return -3;
        }
        slot = place_empty(page, cap);
        if (slot < 0 || !order_admit(o, warm[i].page)) {
            return -2;
        }
        page[slot] = warm[i].page;
    }
    return 0;
}

static void note(Report *out, Step *s, int fault, int victim) {
    s->fault = fault;
    s->victim = victim;
    if (fault) {
        out->faults++;
    }
    if (victim >= 0) {
        out->replaces++;
    }
}

static int serve(Order *o, int *page, int frames, Access ref, Step *s, Report *out) {
    int victim = -1;

    step_blank(s, frames);
    s->page = ref.page;
    s->write = ref.write;
    if (order_has(o, ref.page)) {
        if (!order_hit(o, ref.page)) {
            return -2;
        }
        note(out, s, 0, -1);
    } else if (order_len(o) < frames) {
        int slot = place_empty(page, frames);
        if (slot < 0 || !order_admit(o, ref.page)) {
            return -2;
        }
        page[slot] = ref.page;
        note(out, s, 1, -1);
    } else {
        int slot;
        if (!order_evict(o, &victim)) {
            return -2;
        }
        slot = find_page(page, frames, victim);
        if (slot < 0 || !order_admit(o, ref.page)) {
            return -2;
        }
        page[slot] = ref.page;
        note(out, s, 1, victim);
    }
    snap(o, page, frames, s);
    return 0;
}

int queue_sim(int lru, int frames, const Access *warm, int nwarm, const Access *refs, int nrefs,
              Report *out) {
    Order order;
    int page[FRAME_MAX];
    int i;
    int rc;

    if (!order_open(&order, lru)) {
        return -2;
    }
    for (i = 0; i < frames; i++) {
        page[i] = -1;
    }
    rc = warm_up(&order, page, frames, warm, nwarm);
    if (rc != 0) {
        order_close(&order);
        return rc;
    }
    for (i = 0; i < nrefs; i++) {
        rc = serve(&order, page, frames, refs[i], &out->step[i], out);
        if (rc != 0) {
            order_close(&order);
            return rc;
        }
    }
    order_close(&order);
    return 0;
}
