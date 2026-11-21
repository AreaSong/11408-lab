#include "kernel/k_demand_i.h"
#include "kernel/k_vm.h"

#include "mmu/mmu_fmt.h"
#include "mmu/mmu_i.h"
#include "mmu/mmu_walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int page_key(int pid, uint32_t va) { return pid * 1000003 + (int)(va >> 12); }

static uint32_t frame_pa(int at) { return K_BASE + (uint32_t)at * 4096u; }

static int frame_at(const struct KDemand *d, uint32_t pa) {
    int at;
    if (pa < K_BASE) {
        return -1;
    }
    at = (int)((pa - K_BASE) >> 12);
    if (at < 0 || at >= K_POOL || !d->frame[at].used) {
        return -1;
    }
    return at;
}

static KMap *find_map(KPcb *pcb, uint32_t page) {
    int i;
    for (i = 0; i < pcb->nmap; i++) {
        if (pcb->map[i].va == page) {
            return &pcb->map[i];
        }
    }
    return NULL;
}

static int count_resident(const KPcb *pcb) {
    int n = 0;
    int i;
    for (i = 0; i < pcb->nmap; i++) {
        if (pcb->map[i].demand && pcb->map[i].resident) {
            n++;
        }
    }
    return n;
}

static int read_pte(MmMachine *m, uint32_t va, uint32_t *addr, uint32_t *pte) {
    MmuSplit sp;
    uint32_t pde = 0;
    if (m->mmu == NULL) {
        return -1;
    }
    mmu_split(va, &sp);
    if (mm_raw_read(m, mmu_slot(m->mmu->ptbr, sp.dir), 4, &pde) != 0 || (pde & MMU_P) == 0) {
        return -1;
    }
    *addr = mmu_slot(pde & ~0xfffu, sp.page);
    return mm_raw_read(m, *addr, 4, pte) == 0 ? 0 : -1;
}

static void k_demand_touch(MmMachine *m, uint32_t va, int writing) {
    struct KDemand *d = k_demand_state(m);
    int key;
    (void)writing;
    if (d == NULL || !d->ready || d->algo != K_REPL_LRU || m->kworld->current <= 0) {
        return;
    }
    key = page_key(m->kworld->current, va);
    if (k_order_has(&d->order, key)) {
        k_order_hit(&d->order, key);
    }
}

int k_demand_active(const MmMachine *m) {
    struct KDemand *d = k_demand_state(m);
    return d != NULL && d->ready;
}

void k_demand_off(MmMachine *m) {
    struct KDemand *d;
    if (m == NULL) {
        return;
    }
    m->demand_touch = NULL;
    d = k_demand_state(m);
    if (d == NULL) {
        return;
    }
    if (d->use_order) {
        k_order_close(&d->order);
    }
    k_buddy_close(&d->buddy);
    free(d->swap);
    free(d);
    m->kworld->demand = NULL;
}

int k_demand_on(MmMachine *m, int frames, const char *algo) {
    struct KDemand *d;
    int id = k_repl_id(algo);
    if (!k_proc_live(m) || frames < 1 || frames > K_POOL || id < 0 || id == K_REPL_OPT) {
        return -1;
    }
    k_demand_off(m);
    d = calloc(1, sizeof *d);
    if (d == NULL) {
        return -1;
    }
    d->swap = calloc((size_t)K_SWAP_N, 4096u);
    if (d->swap == NULL || k_buddy_open(&d->buddy, K_POOL) != 0) {
        k_buddy_close(&d->buddy);
        free(d->swap);
        free(d);
        return -1;
    }
    d->use_order = id == K_REPL_FIFO || id == K_REPL_LRU;
    if (d->use_order && !k_order_open(&d->order, id == K_REPL_LRU)) {
        k_buddy_close(&d->buddy);
        free(d->swap);
        free(d);
        return -1;
    }
    d->cap = frames;
    d->algo = id;
    d->ready = 1;
    m->kworld->demand = d;
    m->demand_touch = k_demand_touch;
    return 0;
}

static int alloc_frame(struct KDemand *d, int pid, uint32_t va, int *at) {
    char name[8];
    int start = 0;
    int size = 0;
    snprintf(name, sizeof name, "f%d", d->tick++);
    if (k_buddy_alloc(&d->buddy, name, 1, &start, &size) != 0 || start < 0 || start >= K_POOL) {
        return -1;
    }
    memset(&d->frame[start], 0, sizeof d->frame[start]);
    d->frame[start].used = 1;
    d->frame[start].refs = 1;
    d->frame[start].pid = pid;
    d->frame[start].va = va;
    d->frame[start].pa = frame_pa(start);
    d->frame[start].key = page_key(pid, va);
    memcpy(d->frame[start].name, name, sizeof d->frame[start].name);
    *at = start;
    return 0;
}

static int free_frame(struct KDemand *d, int at) {
    int merges = 0;
    if (at < 0 || !d->frame[at].used) {
        return -1;
    }
    if (d->use_order && !k_order_drop(&d->order, d->frame[at].key)) {
        return -1;
    }
    if (k_buddy_free(&d->buddy, d->frame[at].name, &merges) != 0) {
        return -1;
    }
    d->frame[at].used = 0;
    return 0;
}

static int evict_frame(struct KDemand *d, MmMachine *m, KPcb *pcb, int at) {
    DFrame *fr = &d->frame[at];
    KMap *map = find_map(pcb, fr->va);
    uint32_t addr = 0;
    uint32_t pte = 0;
    uint32_t disk;
    if (map == NULL || read_pte(m, fr->va, &addr, &pte) != 0) {
        return -1;
    }
    if ((pte & MMU_D) != 0) {
        if (map->slot < 0) {
            if (d->next_slot >= K_SWAP_N) {
                return -1;
            }
            map->slot = d->next_slot++;
        }
        memcpy(d->swap + (size_t)map->slot * 4096u, m->mem + fr->pa, 4096u);
        map->in_swap = 1;
    } else {
        map->in_swap = map->slot >= 0;
    }
    disk = map->in_swap ? ((uint32_t)map->slot << 1) : 0;
    if (mm_raw_write(m, addr, 4, disk) != 0) {
        return -1;
    }
    mmu_flush(m);
    map->resident = 0;
    map->pa = 0;
    map->demand = 1;
    if (free_frame(d, at) != 0) {
        return -1;
    }
    d->replaces++;
    return 0;
}

static int pick_order(struct KDemand *d, int pid) {
    int n = k_order_len(&d->order);
    int step = d->algo == K_REPL_LRU ? -1 : 1;
    int i = d->algo == K_REPL_LRU ? n : 1;
    int end = d->algo == K_REPL_LRU ? 0 : n + 1;
    for (; i != end; i += step) {
        int key = k_order_at(&d->order, i);
        int at;
        if (key / 1000003 != pid) {
            continue;
        }
        for (at = 0; at < K_POOL; at++) {
            if (d->frame[at].used && d->frame[at].key == key && d->frame[at].refs <= 1) {
                return at;
            }
        }
    }
    return -1;
}

static int clock_pick(struct KDemand *d, MmMachine *m, KPcb *pcb) {
    int at[K_POOL];
    int n = 0;
    int i;
    int rounds = d->algo == K_REPL_CLOCK2 ? 4 : 2;
    int pass;
    for (i = 0; i < pcb->nmap && n < K_POOL; i++) {
        int idx;
        if (!pcb->map[i].demand || !pcb->map[i].resident) {
            continue;
        }
        idx = frame_at(d, pcb->map[i].pa);
        if (idx >= 0 && d->frame[idx].refs <= 1) {
            at[n++] = idx;
        }
    }
    if (n == 0) {
        return -1;
    }
    for (pass = 0; pass < rounds; pass++) {
        int steps;
        int want_dirty = d->algo == K_REPL_CLOCK2 && (pass % 2) == 1;
        int clear = d->algo == K_REPL_CLOCK || (pass % 2) == 1;
        for (steps = 0; steps < n; steps++) {
            int cur = at[(d->hand + steps) % n];
            uint32_t addr = 0;
            uint32_t pte = 0;
            int acc;
            int dirty;
            if (read_pte(m, d->frame[cur].va, &addr, &pte) != 0) {
                return -1;
            }
            acc = (pte & MMU_A) != 0;
            dirty = (pte & MMU_D) != 0;
            if (!acc && (d->algo == K_REPL_CLOCK || dirty == want_dirty)) {
                d->hand = (d->hand + steps + 1) % n;
                return cur;
            }
            if (clear) {
                mm_raw_write(m, addr, 4, pte & ~MMU_A);
            }
        }
    }
    return -1;
}

static int ensure_room(struct KDemand *d, MmMachine *m, KPcb *pcb) {
    int at;
    if (count_resident(pcb) < d->cap) {
        return 0;
    }
    at = d->use_order ? pick_order(d, pcb->pid) : clock_pick(d, m, pcb);
    if (at < 0) {
        return -1;
    }
    return evict_frame(d, m, pcb, at);
}

static int in_heap(const KPcb *pcb, uint32_t page) {
    return page >= pcb->heap && page < pcb->brk && page < 0x40000000u;
}

static int in_vma(const KPcb *pcb, uint32_t page, int *share) {
    int i;
    for (i = 0; i < pcb->nvma; i++) {
        if (page >= pcb->vma[i].start && page < pcb->vma[i].end) {
            *share = pcb->vma[i].share;
            return 1;
        }
    }
    return 0;
}

static void fill_frame(struct KDemand *d, MmMachine *m, KMap *old, int share, uint32_t pa) {
    uint8_t *dst = m->mem + pa;
    memset(dst, 0, 4096u);
    if (old != NULL && old->in_swap && old->slot >= 0) {
        memcpy(dst, d->swap + (size_t)old->slot * 4096u, 4096u);
        return;
    }
    if (share >= 0 && d->share[share].fd >= 0) {
        DFile *file = &d->file[d->share[share].fd];
        int n = file->len < 4096 ? file->len : 4096;
        if (file->used && n > 0) {
            memcpy(dst, file->bytes, (size_t)n);
        }
    }
}

static int plant_new(MmMachine *m, KPcb *pcb, uint32_t page, uint32_t pa, int shared, int seq) {
    int slot = -1;
    KMap *old = find_map(pcb, page);
    if (old != NULL) {
        slot = old->slot;
    }
    if (k_as_plant(m, pcb, page, pa, shared, 1, seq) != 0) {
        return -1;
    }
    old = find_map(pcb, page);
    if (old == NULL) {
        return -1;
    }
    if (slot >= 0) {
        old->slot = slot;
    }
    return 0;
}

int k_demand_fault(MmMachine *m) {
    struct KDemand *d = k_demand_state(m);
    KPcb *pcb;
    uint32_t page;
    uint32_t pa;
    int share = -1;
    int at = -1;
    KMap *old;
    if (d == NULL || !d->ready || m->kworld->current <= 0) {
        return -1;
    }
    pcb = &m->kworld->pcb[m->kworld->current];
    page = m->badva & ~0xfffu;
    if (page < 0x1000u || (!in_heap(pcb, page) && !in_vma(pcb, page, &share))) {
        return -1;
    }
    if (share >= 0 && d->share[share].resident) {
        at = frame_at(d, d->share[share].pa);
        if (at < 0 || plant_new(m, pcb, page, d->share[share].pa, 1, ++d->tick) != 0) {
            return -1;
        }
        d->frame[at].refs++;
        d->faults++;
        return 0;
    }
    old = find_map(pcb, page);
    if (ensure_room(d, m, pcb) != 0 || alloc_frame(d, pcb->pid, page, &at) != 0) {
        return -1;
    }
    pa = d->frame[at].pa;
    fill_frame(d, m, old, share, pa);
    if (plant_new(m, pcb, page, pa, share >= 0, ++d->tick) != 0) {
        return -1;
    }
    if (share >= 0) {
        d->share[share].resident = 1;
        d->share[share].pa = pa;
    }
    if (d->use_order && !k_order_admit(&d->order, d->frame[at].key)) {
        return -1;
    }
    d->faults++;
    return 0;
}
