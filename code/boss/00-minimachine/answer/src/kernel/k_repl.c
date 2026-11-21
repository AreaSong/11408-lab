#include "kernel/k_vm.h"

#include "kernel/k_order.h"

#include <string.h>

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

int k_repl_id(const char *name) {
    if (name == NULL) {
        return -1;
    }
    if (strcmp(name, "fifo") == 0) {
        return K_REPL_FIFO;
    }
    if (strcmp(name, "lru") == 0) {
        return K_REPL_LRU;
    }
    if (strcmp(name, "opt") == 0) {
        return K_REPL_OPT;
    }
    if (strcmp(name, "clock") == 0) {
        return K_REPL_CLOCK;
    }
    if (strcmp(name, "clock2") == 0) {
        return K_REPL_CLOCK2;
    }
    return -1;
}

const char *k_repl_name(int id) {
    if (id == K_REPL_FIFO) {
        return "fifo";
    }
    if (id == K_REPL_LRU) {
        return "lru";
    }
    if (id == K_REPL_OPT) {
        return "opt";
    }
    if (id == K_REPL_CLOCK) {
        return "clock";
    }
    if (id == K_REPL_CLOCK2) {
        return "clock2";
    }
    return "unknown";
}

static int place_empty(const int *page, int cap) {
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

static void snap_order(const KOrder *order, const int *page, int cap, KStep *s) {
    int n = k_order_len(order);
    int i;
    for (i = 0; i < cap; i++) {
        s->slots[i] = page[i];
    }
    s->norder = n;
    for (i = 0; i < n && i < K_VM_FRAMES; i++) {
        s->order[i] = k_order_at(order, i + 1);
    }
}

static void note_fault(KReport *out, KStep *s, int victim) {
    s->fault = 1;
    s->victim = victim;
    out->faults++;
    if (victim >= 0) {
        out->replaces++;
    }
}

static int warm_order(KOrder *order, int *page, int cap, const KAccess *warm, int nwarm) {
    int i;
    for (i = 0; i < nwarm; i++) {
        int slot;
        if (k_order_has(order, warm[i].page)) {
            return -3;
        }
        slot = place_empty(page, cap);
        if (slot < 0 || !k_order_admit(order, warm[i].page)) {
            return -2;
        }
        page[slot] = warm[i].page;
    }
    return 0;
}

static int serve_order(KOrder *order, int *page, int frames, KAccess ref, KStep *s, KReport *out) {
    step_blank(s, frames);
    s->page = ref.page;
    s->write = ref.write;
    if (k_order_has(order, ref.page)) {
        if (!k_order_hit(order, ref.page)) {
            return -2;
        }
        s->fault = 0;
    } else if (k_order_len(order) < frames) {
        int slot = place_empty(page, frames);
        if (slot < 0 || !k_order_admit(order, ref.page)) {
            return -2;
        }
        page[slot] = ref.page;
        note_fault(out, s, -1);
    } else {
        int victim = k_order_at(order, order->front_mru ? k_order_len(order) : 1);
        int slot;
        if (!k_order_drop(order, victim)) {
            return -2;
        }
        slot = find_page(page, frames, victim);
        if (slot < 0 || !k_order_admit(order, ref.page)) {
            return -2;
        }
        page[slot] = ref.page;
        note_fault(out, s, victim);
    }
    snap_order(order, page, frames, s);
    return 0;
}

static int queue_sim(int lru, int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs,
                     KReport *out) {
    KOrder order;
    int page[K_VM_FRAMES];
    int i;
    int rc;
    if (!k_order_open(&order, lru)) {
        return -2;
    }
    for (i = 0; i < frames; i++) {
        page[i] = -1;
    }
    rc = warm_order(&order, page, frames, warm, nwarm);
    for (i = 0; rc == 0 && i < nrefs; i++) {
        rc = serve_order(&order, page, frames, refs[i], &out->step[i], out);
    }
    k_order_close(&order);
    return rc;
}

static int next_use(const KAccess *refs, int n, int from, int page) {
    int i;
    for (i = from; i < n; i++) {
        if (refs[i].page == page) {
            return i;
        }
    }
    return n;
}

static int opt_victim(const int *page, int frames, const KAccess *refs, int n, int future) {
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

static int opt_sim(int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs, KReport *out) {
    int page[K_VM_FRAMES];
    int i;
    for (i = 0; i < frames; i++) {
        page[i] = -1;
    }
    for (i = 0; i < nwarm; i++) {
        int slot = place_empty(page, frames);
        if (slot < 0 || find_page(page, frames, warm[i].page) >= 0) {
            return find_page(page, frames, warm[i].page) >= 0 ? -3 : -2;
        }
        page[slot] = warm[i].page;
    }
    for (i = 0; i < nrefs; i++) {
        KStep *s = &out->step[i];
        int slot = find_page(page, frames, refs[i].page);
        step_blank(s, frames);
        s->page = refs[i].page;
        s->write = refs[i].write;
        if (slot >= 0) {
            s->fault = 0;
        } else {
            int victim = -1;
            slot = place_empty(page, frames);
            if (slot < 0) {
                slot = opt_victim(page, frames, refs, nrefs, i + 1);
                if (slot < 0) {
                    return -2;
                }
                victim = page[slot];
            }
            page[slot] = refs[i].page;
            note_fault(out, s, victim);
        }
        for (slot = 0; slot < frames; slot++) {
            s->slots[slot] = page[slot];
        }
    }
    return 0;
}

int k_repl_run(const char *algo, int frames, const KAccess *warm, int nwarm, const KAccess *refs, int nrefs,
               KReport *out) {
    int id = k_repl_id(algo);
    int rc;
    if (id < 0) {
        return -1;
    }
    if (frames < 1 || frames > K_VM_FRAMES || nrefs < 1 || nrefs > K_VM_REFS || nwarm < 0 || nwarm > frames ||
        (nwarm > 0 && warm == NULL) || refs == NULL || out == NULL) {
        return -2;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->algo, algo, strlen(algo) + 1);
    out->frames = frames;
    out->n = nrefs;
    if (id == K_REPL_OPT) {
        rc = opt_sim(frames, warm, nwarm, refs, nrefs, out);
    } else if (id == K_REPL_FIFO || id == K_REPL_LRU) {
        rc = queue_sim(id == K_REPL_LRU, frames, warm, nwarm, refs, nrefs, out);
    } else {
        rc = k_clock_sim(id == K_REPL_CLOCK2, frames, warm, nwarm, refs, nrefs, out);
    }
    return rc;
}

int k_lru_demo(int *oldest) {
    KOrder order;
    int victim = -1;
    int ok;
    if (oldest == NULL || !k_order_open(&order, 1)) {
        return 0;
    }
    ok = k_order_admit(&order, 1) && k_order_admit(&order, 2) && k_order_links(&order) && k_order_at(&order, 1) == 2 &&
         k_order_hit(&order, 1) && k_order_at(&order, 1) == 1 && k_order_at(&order, 2) == 2;
    if (ok) {
        victim = k_order_at(&order, k_order_len(&order));
        ok = k_order_drop(&order, victim);
    }
    k_order_close(&order);
    *oldest = victim;
    return ok;
}
