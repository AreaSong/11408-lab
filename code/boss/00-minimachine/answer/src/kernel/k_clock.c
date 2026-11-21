#include "kernel/k_vm.h"

#include "dlist.h"

#include <string.h>

/*
 * CLOCK 的环用循环双链表串页框号。页留在数组里，指针往前走，页不搬家。
 * 简单 CLOCK 见到访问位 1 就清掉再走，见到 0 就换。
 * 改进型先找 (0,0)，再找 (0,1) 并顺手清访问位。第一轮不能清，否则分不出本来就是 0 的页。
 */

typedef struct {
    DList ring;
    int page[K_VM_FRAMES];
    int ref[K_VM_FRAMES];
    int dirty[K_VM_FRAMES];
    int cap;
    int used;
    int hand;
} KRing;

static void step_blank(KStep *s, int frames) {
    int i;
    memset(s, 0, sizeof *s);
    s->victim = -1;
    s->hand = -1;
    for (i = 0; i < frames; i++) {
        s->slots[i] = -1;
        s->refbit[i] = -1;
        s->dirty[i] = -1;
        s->order[i] = -1;
    }
}

static int ring_open(KRing *g, int frames) {
    int i;
    memset(g, 0, sizeof *g);
    g->cap = frames;
    g->hand = 1;
    for (i = 0; i < K_VM_FRAMES; i++) {
        g->page[i] = -1;
    }
    return dlist_init(&g->ring, true) ? 1 : 0;
}

static int ring_find(KRing *g, int page, int *fi) {
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

static int ring_place(KRing *g, int page, int aref, int dirty) {
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

static void ring_advance(KRing *g) { g->hand = g->hand % g->used + 1; }

static void ring_take(KRing *g, int fi, int page, int write, int *victim) {
    *victim = g->page[fi];
    g->page[fi] = page;
    g->ref[fi] = 1;
    g->dirty[fi] = write;
    ring_advance(g);
}

static int replace_clock(KRing *g, int page, int write, int *victim) {
    int guard;
    for (guard = 0; guard < g->used + 1; guard++) {
        int fi = 0;
        if (!dlist_get(&g->ring, g->hand, &fi)) {
            return 0;
        }
        if (g->ref[fi] == 0) {
            ring_take(g, fi, page, write, victim);
            return 1;
        }
        g->ref[fi] = 0;
        ring_advance(g);
    }
    return 0;
}

static int replace_clock2(KRing *g, int page, int write, int *victim, int *passes) {
    int pass;
    for (pass = 0; pass < 4; pass++) {
        int want_dirty = pass % 2;
        int clear = pass % 2;
        int steps;
        for (steps = 0; steps < g->used; steps++) {
            int fi = 0;
            if (!dlist_get(&g->ring, g->hand, &fi)) {
                return 0;
            }
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

static void ring_snap(const KRing *g, KStep *s) {
    int i;
    for (i = 0; i < g->cap; i++) {
        s->slots[i] = g->page[i];
        s->refbit[i] = g->page[i] < 0 ? -1 : g->ref[i];
        s->dirty[i] = g->page[i] < 0 ? -1 : g->dirty[i];
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

static int ring_miss(KRing *g, int improved, int page, int write, int *victim, int *passes) {
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

static int run_refs(KRing *g, int improved, const KAccess *refs, int nrefs, KReport *out) {
    int i;
    for (i = 0; i < nrefs; i++) {
        KStep *s = &out->step[i];
        int fi = 0;
        int victim = -1;
        int passes = 0;
        step_blank(s, g->cap);
        s->page = refs[i].page;
        s->write = refs[i].write;
        if (ring_find(g, refs[i].page, &fi)) {
            g->ref[fi] = 1;
            if (refs[i].write) {
                g->dirty[fi] = 1;
            }
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

int k_clock_sim(int improved, int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs,
                KReport *out) {
    KRing g;
    int i;
    int rc = 0;
    if (!ring_open(&g, frames)) {
        return -2;
    }
    for (i = 0; i < nwarm; i++) {
        int fi = 0;
        if (ring_find(&g, warm[i].page, &fi) || !ring_place(&g, warm[i].page, 1, warm[i].write)) {
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

int k_clock_once(const char *algo, const int *page, const int *aref, const int *dirty, int n, int next,
                 int write, KReport *out) {
    KRing g;
    int improved = k_repl_id(algo) == K_REPL_CLOCK2;
    int i;
    KAccess one;
    if (k_repl_id(algo) != K_REPL_CLOCK && !improved) {
        return -1;
    }
    if (page == NULL || aref == NULL || dirty == NULL || out == NULL || n < 1 || n > K_VM_FRAMES) {
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
    one.page = next;
    one.write = write;
    if (!run_refs(&g, improved, &one, 1, out)) {
        dlist_destroy(&g.ring);
        return -2;
    }
    dlist_destroy(&g.ring);
    return 0;
}
