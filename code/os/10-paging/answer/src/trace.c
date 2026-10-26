#include "trace.h"

#include "addr.h"
#include "io.h"

#include <string.h>

static int find_page(const Trace *t, unsigned long long page) {
    int i;

    for (i = 0; i < t->npage; i++) {
        if (t->pages[i].page == page) {
            return i;
        }
    }
    return -1;
}

static int valid_count(const Trace *t) {
    int i;
    int n = 0;

    for (i = 0; i < t->npage; i++) {
        n += t->pages[i].valid;
    }
    return n;
}

static int find_tlb(const Trace *t, unsigned long long page) {
    int i;

    for (i = 0; i < TRACE_TLB; i++) {
        if (t->tlb[i].used && t->tlb[i].page == page) {
            return i;
        }
    }
    return -1;
}

static void drop_tlb(Trace *t, unsigned long long page) {
    int i;

    for (i = 0; i < TRACE_TLB; i++) {
        if (t->tlb[i].used && t->tlb[i].page == page) {
            t->tlb[i].used = 0;
        }
    }
}

static int pick_tlb(const Trace *t) {
    int i;
    int best = -1;

    for (i = 0; i < TRACE_TLB; i++) {
        if (!t->tlb[i].used) {
            return i;
        }
        if (best < 0 || t->tlb[i].stamp < t->tlb[best].stamp) {
            best = i;
        }
    }
    return best;
}

static int pick_victim(const Trace *t) {
    int i;
    int best = -1;

    for (i = 0; i < t->npage; i++) {
        if (!t->pages[i].valid) {
            continue;
        }
        if (best < 0 || t->pages[i].stamp < t->pages[best].stamp ||
            (t->pages[i].stamp == t->pages[best].stamp && t->pages[i].page < t->pages[best].page)) {
            best = i;
        }
    }
    return best;
}

static void stamp(Trace *t, int page_i, int tlb_i) {
    unsigned long long now = ++t->clock;

    if (page_i >= 0) {
        t->pages[page_i].stamp = now;
    }
    if (tlb_i >= 0) {
        t->tlb[tlb_i].stamp = now;
    }
}

static int install_tlb(Trace *t, unsigned long long page, unsigned long long frame) {
    int slot = find_tlb(t, page);

    if (slot < 0) {
        slot = pick_tlb(t);
    }
    if (slot < 0) {
        return -1;
    }
    t->tlb[slot].used = 1;
    t->tlb[slot].page = page;
    t->tlb[slot].frame = frame;
    stamp(t, find_page(t, page), slot);
    return slot;
}

int trace_init(Trace *t, unsigned long long page_bytes, unsigned long long tlb_ns,
               unsigned long long mem_ns, unsigned long long fault_ns, int resident) {
    int bits = 0;

    if (pow2_bits(page_bytes, &bits) != 0 || bits < 1) {
        return -1;
    }
    if (resident < 1 || resident > TRACE_PAGES) {
        return -1;
    }
    memset(t, 0, sizeof *t);
    t->page_bytes = page_bytes;
    t->tlb_ns = tlb_ns;
    t->mem_ns = mem_ns;
    t->fault_ns = fault_ns;
    t->resident = resident;
    return 0;
}

int trace_map(Trace *t, unsigned long long page, unsigned long long frame, int valid) {
    if (valid != 0 && valid != 1) {
        return -4;
    }
    if (find_page(t, page) >= 0) {
        return -1;
    }
    if (t->npage >= TRACE_PAGES) {
        return -2;
    }
    if (valid && valid_count(t) >= t->resident) {
        return -3;
    }
    t->pages[t->npage].page = page;
    t->pages[t->npage].frame = frame;
    t->pages[t->npage].valid = valid;
    t->pages[t->npage].stamp = 0;
    t->npage++;
    return 0;
}

static int finish_pa(const Trace *t, GoInfo *out) {
    return addr_join(t->page_bytes, out->frame, out->offset, &out->pa);
}

static int charge(unsigned long long a, unsigned long long b, unsigned long long *ns) {
    return add_ull(a, b, ns);
}

static int on_hit(Trace *t, int page_i, int tlb_i, GoInfo *out) {
    out->result = GO_TLB;
    out->frame = t->tlb[tlb_i].frame;
    out->victim = -1;
    if (charge(t->tlb_ns, t->mem_ns, &out->ns) != 0) {
        return -1;
    }
    stamp(t, page_i, tlb_i);
    return finish_pa(t, out);
}

static int on_resident(Trace *t, int page_i, GoInfo *out) {
    unsigned long long tables;

    out->result = GO_PT;
    out->frame = t->pages[page_i].frame;
    out->victim = -1;
    if (charge(t->mem_ns, t->mem_ns, &tables) != 0 || charge(t->tlb_ns, tables, &out->ns) != 0) {
        return -1;
    }
    if (install_tlb(t, out->page, out->frame) < 0 || finish_pa(t, out) != 0) {
        return -1;
    }
    return 0;
}

static int take_frame(Trace *t, int page_i, GoInfo *out) {
    int live = valid_count(t);

    if (live < t->resident) {
        out->frame = t->pages[page_i].frame;
        out->victim = -1;
        return 0;
    }
    {
        int victim = pick_victim(t);
        if (victim < 0) {
            return -1;
        }
        out->victim = (long long)t->pages[victim].page;
        out->frame = t->pages[victim].frame;
        drop_tlb(t, t->pages[victim].page);
        t->pages[victim].valid = 0;
    }
    return 0;
}

static int on_fault(Trace *t, int page_i, GoInfo *out) {
    unsigned long long retry;

    out->result = GO_FAULT;
    /* 先查快表、再读页表发现不在内存，然后是缺页处理。处理时间已含更新。 */
    if (charge(t->tlb_ns, t->mem_ns, &out->ns) != 0 || charge(out->ns, t->fault_ns, &out->ns) != 0) {
        return -1;
    }
    if (take_frame(t, page_i, out) != 0) {
        return -1;
    }
    t->pages[page_i].frame = out->frame;
    t->pages[page_i].valid = 1;
    if (install_tlb(t, out->page, out->frame) < 0) {
        return -1;
    }
    if (add_ull(t->tlb_ns, t->mem_ns, &retry) != 0 || add_ull(out->ns, retry, &out->ns) != 0) {
        return -1;
    }
    return finish_pa(t, out);
}

int trace_go(Trace *t, unsigned long long addr, GoInfo *out) {
    int page_i;
    int tlb_i;

    memset(out, 0, sizeof *out);
    out->victim = -1;
    out->page = addr / t->page_bytes;
    out->offset = addr % t->page_bytes;
    page_i = find_page(t, out->page);
    if (page_i < 0) {
        out->result = GO_BOUND;
        return 0;
    }
    tlb_i = find_tlb(t, out->page);
    if (tlb_i >= 0) {
        return on_hit(t, page_i, tlb_i, out);
    }
    if (t->pages[page_i].valid) {
        return on_resident(t, page_i, out);
    }
    return on_fault(t, page_i, out);
}
