#include "kernel/k_i.h"

#include "asm/asm_image.h"
#include "cpu/cpu_decode.h"
#include "mmu/mmu_i.h"
#include "mmu/mmu_walk.h"

#include <stdlib.h>
#include <string.h>

static int alloc_page(MmMachine *m, uint32_t *pa) {
    uint32_t page;
    if (m->mmu == NULL) {
        return -1;
    }
    page = m->mmu->next_pt;
    if (page < 0x00081000u || page + MMU_PAGE_BYTES > 0x000C0000u || page + MMU_PAGE_BYTES > m->mem_size) {
        return -1;
    }
    memset(m->mem + page, 0, MMU_PAGE_BYTES);
    m->mmu->next_pt = page + MMU_PAGE_BYTES;
    *pa = page;
    return 0;
}

static int remember(KPcb *pcb, uint32_t va, uint32_t pa) {
    if (pcb->nmap >= K_PAGE_MAX) {
        return -1;
    }
    pcb->map[pcb->nmap].va = va;
    pcb->map[pcb->nmap].pa = pa;
    pcb->nmap++;
    return 0;
}

/* 临时改页目录基址，建完别人的页表再改回来。TLB 里没有进程号，所以调用方切换时要刷。 */
static int map_at(MmMachine *m, uint32_t pd, uint32_t va, uint32_t pa) {
    uint32_t saved;
    int rc;
    if (va < 0x1000u || va >= 0x80000000u || (va & 0xfffu) != 0 || (pa & 0xfffu) != 0) {
        return -1;
    }
    saved = m->mmu->ptbr;
    m->mmu->ptbr = pd;
    rc = mmu_map(m, va, pa, 1, 1);
    m->mmu->ptbr = saved;
    return rc == 0 ? 0 : -1;
}

static int map_stack(MmMachine *m, KPcb *pcb) {
    uint32_t pa = 0;
    if (alloc_page(m, &pa) != 0 || map_at(m, pcb->ptbr, K_STACK_VA, pa) != 0) {
        return -1;
    }
    return remember(pcb, K_STACK_VA, pa);
}

static int map_low(MmMachine *m, KPcb *pcb) {
    uint32_t last = 0x1000u;
    uint32_t addr;
    uint32_t va;
    uint32_t end;
    for (addr = 0x1000u; addr < 0x10000u && addr < m->mem_size; addr++) {
        if (m->mem[addr] != 0) {
            last = addr;
        }
    }
    end = (last & ~0xfffu) + 0x1000u;
    if (end < 0x2000u) {
        end = 0x2000u;
    }
    for (va = 0x1000u; va < end; va += 0x1000u) {
        if (map_at(m, pcb->ptbr, va, va) != 0 || remember(pcb, va, va) != 0) {
            return -1;
        }
    }
    return 0;
}

static int make_idle(MmMachine *m, KWorld *w) {
    KPcb *pcb = &w->pcb[0];
    uint32_t pd = 0;
    uint32_t code = 0;
    uint32_t word;
    if (alloc_page(m, &pd) != 0 || alloc_page(m, &code) != 0) {
        return -1;
    }
    word = cpu_enc_b(MM_OP_JCC, MM_COND_JMP, (uint16_t)(int16_t)-4);
    if (mm_raw_write(m, code, 4, word) != 0) {
        return -1;
    }
    pcb->used = 1;
    pcb->pid = 0;
    pcb->state = K_ST_READY;
    pcb->ptbr = pd;
    pcb->pc = 0x1000u;
    pcb->psw = K_USER_PSW;
    if (map_at(m, pd, 0x1000u, code) != 0 || remember(pcb, 0x1000u, code) != 0) {
        return -1;
    }
    return 0;
}

static int make_init(MmMachine *m, KWorld *w) {
    KPcb *pcb = &w->pcb[1];
    int i;
    pcb->used = 1;
    pcb->pid = 1;
    pcb->state = K_ST_RUN;
    pcb->ptbr = m->mmu->ptbr;
    pcb->pc = m->pc;
    pcb->psw = K_USER_PSW;
    pcb->born = m->cycles;
    for (i = 0; i < 8; i++) {
        pcb->r[i] = m->r[i];
    }
    pcb->r[7] = K_STACK_SP;
    if (map_low(m, pcb) != 0 || map_stack(m, pcb) != 0) {
        return -1;
    }
    m->r[7] = K_STACK_SP;
    m->psw = K_USER_PSW;
    m->sr[MM_SR_PTBR] = pcb->ptbr;
    return 0;
}

int k_as_setup(MmMachine *m, KWorld *w) {
    MmuCfg cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.entries = 8;
    cfg.ways = 8;
    cfg.replace = MMU_REPL_LRU;
    cfg.walk_cycles = 1;
    cfg.ptbr = 0x00080000u;
    mmu_drop(m);
    if (mmu_enable(m, &cfg) != 0) {
        return -1;
    }
    if (make_init(m, w) != 0 || make_idle(m, w) != 0) {
        mmu_drop(m);
        return -1;
    }
    w->current = 1;
    return 0;
}

int k_as_copy(MmMachine *m, KPcb *dst, const KPcb *src) {
    uint32_t pd = 0;
    int i;
    if (alloc_page(m, &pd) != 0) {
        return -1;
    }
    dst->ptbr = pd;
    dst->nmap = 0;
    for (i = 0; i < src->nmap; i++) {
        uint32_t pa = 0;
        /* 共享页不复制。父子改的是同一个计数器，竞态和自旋锁才看得到对方。 */
        if (src->map[i].shared) {
            pa = src->map[i].pa;
        } else if (alloc_page(m, &pa) != 0) {
            return -1;
        } else {
            memcpy(m->mem + pa, m->mem + src->map[i].pa, MMU_PAGE_BYTES);
        }
        if (map_at(m, pd, src->map[i].va, pa) != 0 || remember(dst, src->map[i].va, pa) != 0) {
            return -1;
        }
        dst->map[dst->nmap - 1].shared = src->map[i].shared;
    }
    return 0;
}

int k_share(MmMachine *m, uint32_t va) {
    KWorld *w;
    KPcb *pcb;
    uint32_t page;
    uint32_t pa = 0;
    int i;
    if (!k_proc_live(m)) {
        return -1;
    }
    w = m->kworld;
    if (w->current <= 0) {
        return -1;
    }
    page = va & ~(MMU_PAGE_BYTES - 1u);
    if (page < 0x1000u || page >= 0x80000000u) {
        return -1;
    }
    pcb = &w->pcb[w->current];
    for (i = 0; i < pcb->nmap; i++) {
        if (pcb->map[i].va == page) {
            pcb->map[i].shared = 1;
            return 0;
        }
    }
    if (alloc_page(m, &pa) != 0 || map_at(m, pcb->ptbr, page, pa) != 0 || remember(pcb, page, pa) != 0) {
        return -1;
    }
    pcb->map[pcb->nmap - 1].shared = 1;
    return 0;
}

int k_as_exec(MmMachine *m, KPcb *pcb, const KImage *image) {
    uint32_t pd = 0;
    int i;
    if (alloc_page(m, &pd) != 0) {
        return -1;
    }
    pcb->ptbr = pd;
    pcb->nmap = 0;
    for (i = 0; i < image->npages; i++) {
        uint32_t pa = 0;
        if (image->va[i] < 0x1000u) {
            continue;
        }
        if (alloc_page(m, &pa) != 0) {
            return -1;
        }
        memcpy(m->mem + pa, image->bytes[i], MMU_PAGE_BYTES);
        if (map_at(m, pd, image->va[i], pa) != 0 || remember(pcb, image->va[i], pa) != 0) {
            return -1;
        }
    }
    if (map_stack(m, pcb) != 0) {
        return -1;
    }
    memset(pcb->r, 0, sizeof pcb->r);
    pcb->r[7] = K_STACK_SP;
    pcb->pc = image->entry;
    pcb->psw = K_USER_PSW;
    return 0;
}

static void free_image(KImage *image) {
    int i;
    for (i = 0; i < image->npages; i++) {
        free(image->bytes[i]);
    }
    memset(image, 0, sizeof *image);
}

const KImage *k_image_find(const KWorld *w, const char *name) {
    int i;
    if (w == NULL || name == NULL) {
        return NULL;
    }
    for (i = 0; i < w->nimage; i++) {
        if (strcmp(w->image[i].name, name) == 0) {
            return &w->image[i];
        }
    }
    return NULL;
}

static int take_page(const struct AsmImage *img, uint32_t page, KImage *image) {
    uint32_t i;
    int any = 0;
    uint8_t *buf;
    for (i = 0; i < MMU_PAGE_BYTES && page + i < img->space; i++) {
        if (img->mark[page + i]) {
            any = 1;
        }
    }
    if (!any || page < 0x1000u) {
        return 0;
    }
    if (image->npages >= K_IMAGE_PAGE) {
        return -1;
    }
    buf = calloc(MMU_PAGE_BYTES, 1);
    if (buf == NULL) {
        return -1;
    }
    for (i = 0; i < MMU_PAGE_BYTES && page + i < img->space; i++) {
        if (img->mark[page + i]) {
            buf[i] = img->mem[page + i];
        }
    }
    image->va[image->npages] = page;
    image->bytes[image->npages] = buf;
    image->npages++;
    return 1;
}

int k_image_add(MmMachine *m, const char *name, const struct AsmImage *img) {
    KWorld *w;
    KImage *image;
    uint32_t page;
    if (!k_proc_live(m) || name == NULL || img == NULL || name[0] == '\0' || strlen(name) >= 16) {
        return -1;
    }
    w = m->kworld;
    if (w->nimage >= K_IMAGE_MAX) {
        return -1;
    }
    image = &w->image[w->nimage];
    memset(image, 0, sizeof *image);
    snprintf(image->name, sizeof image->name, "%s", name);
    image->entry = img->entry;
    for (page = 0; page < img->space && page < 0x10000u; page += MMU_PAGE_BYTES) {
        int took = take_page(img, page, image);
        if (took < 0) {
            free_image(image);
            return -1;
        }
    }
    if (image->npages == 0) {
        free_image(image);
        return -1;
    }
    w->nimage++;
    return 0;
}
