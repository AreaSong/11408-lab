#include "sim_priv.h"

#include "dlist.h"

#include <string.h>

/*
 * CLOCK 的环用循环双链表串起页框号。页本身留在数组里，指针前移，页不搬家。
 * 简单 CLOCK 见到访问位 1 就清掉再往前走，见到 0 就换掉。
 * 改进型先找 (0,0)，再找 (0,1) 并顺手清访问位。两轮都不成，访问位已经全是 0，
 * 再重复这两轮，一定能找到。第一轮不能清访问位，否则分不出「本来就没被用」和「被我清掉的」。
 */

typedef struct {
    DList ring;
    int page[FRAME_MAX];
    int ref[FRAME_MAX];
    int dirty[FRAME_MAX];
    int cap;
    int used;
    int hand;
} Ring;

static int ring_open(Ring *g, int frames) {
    int i;

    memset(g, 0, sizeof *g);
    g->cap = frames;
    g->hand = 1;
    for (i = 0; i < FRAME_MAX; i++) {
        g->page[i] = -1;
    }
    if (!dlist_init(&g->ring, true)) {
        return 0;
    }
    return 1;
}

static int ring_find(Ring *g, int page, int *fi) {
    int n = dlist_length(&g->ring);
    int i;

    for (i = 1; i <= n; i++) {
        int idx = 0;
        if (!dlist_get(&g->ring, i, &idx)) {
            return 0;
        }
        if (g->page[idx] == page) {
            *fi = idx;
            return 1;
        }
    }
    return 0;
}

static int ring_place(Ring *g, int page, int aref, int dirty) {
    int fi = g->used;

    g->page[fi] = page;
    g->ref[fi] = aref;
    g->dirty[fi] = dirty;
    if (!dlist_insert(&g->ring, g->used + 1, fi)) {
        return 0;
    }
    g->used++;
    return 1;
}

static void ring_advance(Ring *g) {
    g->hand = g->hand % g->used + 1;
}

static void ring_take(Ring *g, int fi, int page, int write, int *victim) {
    *victim = g->page[fi];
    g->page[fi] = page;
    g->ref[fi] = 1;
    g->dirty[fi] = write;
    ring_advance(g);
}

static int replace_clock(Ring *g, int page, int write, int *victim) {
    int guard;
    int n = g->used;

    for (guard = 0; guard < n + 1; guard++) {
        int fi = 0;
        dlist_get(&g->ring, g->hand, &fi);
        if (g->ref[fi] == 0) {
            ring_take(g, fi, page, write, victim);
            return 1;
        }
        g->ref[fi] = 0;
        ring_advance(g);
    }
    return 0;
}

static int replace_clock2(Ring *g, int page, int write, int *victim, int *passes) {
    int pass;
    int n = g->used;

    for (pass = 0; pass < 4; pass++) {
        int want_dirty = pass % 2;
        int clear = pass % 2;
        int steps;

        for (steps = 0; steps < n; steps++) {
            int fi = 0;
            dlist_get(&g->ring, g->hand, &fi);
            if (g->ref[fi] == 0 && g->dirty[fi] == want_dirty) {
                *passes = pass + 1;
                ring_take(g, fi, page, write, victim);
                return 1;
            }
            if (clear) {
                g->ref[fi] = 0;
            }
            ring_advance(g);
        }
    }
    return 0;
}

static void ring_snap(const Ring *g, Step *s) {
    int i;

    for (i = 0; i < g->cap; i++) {
        s->slots[i] = g->page[i];
        if (g->page[i] < 0) {
            s->refbit[i] = -1;
            s->dirty[i] = -1;
        } else {
            s->refbit[i] = g->ref[i];
            s->dirty[i] = g->dirty[i];
        }
    }
    s->norder = g->used;
    for (i = 1; i <= g->used; i++) {
        int idx = 0;
        dlist_get(&g->ring, i, &idx);
        s->order[i - 1] = g->page[idx];
    }
    if (g->used == 0) {
        s->hand = -1;
    } else {
        int idx = 0;
        dlist_get(&g->ring, g->hand, &idx);
        s->hand = g->page[idx];
    }
}

static int ring_hit(Ring *g, int fi, int write) {
    g->ref[fi] = 1;
    if (write) {
        g->dirty[fi] = 1;
    }
    return 1;
}

static int ring_miss(Ring *g, int improved, int page, int write, int *victim, int *passes) {
    *victim = -1;
    *passes = 0;
    if (g->used < g->cap) {
        return ring_place(g, page, 1, write);
    }
    if (improved) {
        return replace_clock2(g, page, write, victim, passes);
    }
    return replace_clock(g, page, write, victim);
}

static int run_refs(Ring *g, int improved, const Access *refs, int nrefs, Report *out) {
    int i;

    for (i = 0; i < nrefs; i++) {
        Step *s = &out->step[i];
        int fi = 0;
        int victim = -1;
        int passes = 0;

        step_blank(s, g->cap);
        s->page = refs[i].page;
        s->write = refs[i].write;
        if (ring_find(g, refs[i].page, &fi)) {
            ring_hit(g, fi, refs[i].write);
            s->fault = 0;
        } else {
            if (!ring_miss(g, improved, refs[i].page, refs[i].write, &victim, &passes)) {
                return 0;
            }
            s->fault = 1;
            s->victim = victim;
            s->passes = passes;
            out->faults++;
            if (victim >= 0) {
                out->replaces++;
            }
        }
        ring_snap(g, s);
    }
    return 1;
}

int clock_sim(int improved, int frames, const Access *warm, int nwarm, const Access *refs,
              int nrefs, Report *out) {
    Ring g;
    int i;
    int rc = 0;

    if (!ring_open(&g, frames)) {
        return -2;
    }
    for (i = 0; i < nwarm; i++) {
        int fi = 0;
        if (ring_find(&g, warm[i].page, &fi) || !ring_place(&g, warm[i].page, 1, 0)) {
            rc = ring_find(&g, warm[i].page, &fi) ? -3 : -2;
            dlist_destroy(&g.ring);
            return rc;
        }
    }
    if (!run_refs(&g, improved, refs, nrefs, out)) {
        rc = -2;
    }
    dlist_destroy(&g.ring);
    return rc;
}

int clock_preset(const char *algo, const int *page, const int *aref, const int *dirty, int n,
                 Access next, Report *out) {
    Ring g;
    int improved = algo_id(algo) == 4;
    int i;
    Access one;

    if (algo_id(algo) != 3 && algo_id(algo) != 4) {
        return -1;
    }
    if (n < 1 || n > FRAME_MAX) {
        return -2;
    }
    if (!ring_open(&g, n)) {
        return -2;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->algo, algo, strlen(algo) + 1);
    out->frames = n;
    out->n = 1;
    for (i = 0; i < n; i++) {
        int fi = 0;
        if (ring_find(&g, page[i], &fi) || !ring_place(&g, page[i], aref[i], dirty[i])) {
            int rc = ring_find(&g, page[i], &fi) ? -3 : -2;
            dlist_destroy(&g.ring);
            return rc;
        }
    }
    one = next;
    if (!run_refs(&g, improved, &one, 1, out)) {
        dlist_destroy(&g.ring);
        return -2;
    }
    dlist_destroy(&g.ring);
    return 0;
}

int clock_ring_ok(void) {
    DList ring;
    int i;
    int ok;
    int mid = -1;

    if (!dlist_init(&ring, true)) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (!dlist_insert(&ring, i + 1, i)) {
            dlist_destroy(&ring);
            return 0;
        }
    }
    ok = dlist_links_ok(&ring) && dlist_length(&ring) == 3 && dlist_get(&ring, 2, &mid) && mid == 1;
    dlist_destroy(&ring);
    return ok;
}
