#include "kernel/k_demand_i.h"
#include "kernel/k_vm.h"

#include "cpu/cpu_intr.h"

#include <stdint.h>
#include <string.h>

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

static int do_sbrk(KPcb *pcb, uint32_t arg, uint32_t *old) {
    int32_t delta = (int32_t)arg;
    int64_t next = (int64_t)pcb->brk + delta;
    if (pcb->heap == 0 || next < (int64_t)pcb->heap || next > 0x40000000LL) {
        return -1;
    }
    *old = pcb->brk;
    pcb->brk = (uint32_t)next;
    return 0;
}

static int do_mmap(struct KDemand *d, KPcb *pcb, uint32_t fd, uint32_t len, uint32_t *addr) {
    int si;
    int file = -1;
    if (len == 0 || len > 4096u || pcb->nvma >= K_VMA_MAX) {
        return -1;
    }
    if (fd != 0xffffffffu) {
        if (fd >= K_FILE_N || !d->file[fd].used) {
            return -1;
        }
        file = (int)fd;
    }
    for (si = 0; si < K_SHARE_N; si++) {
        if (!d->share[si].used) {
            break;
        }
    }
    if (si == K_SHARE_N) {
        return -1;
    }
    if (pcb->mmap_next < 0x40000000u) {
        pcb->mmap_next = 0x40000000u;
    }
    *addr = pcb->mmap_next;
    memset(&d->share[si], 0, sizeof d->share[si]);
    d->share[si].used = 1;
    d->share[si].fd = file;
    pcb->vma[pcb->nvma].start = *addr;
    pcb->vma[pcb->nvma].end = *addr + len;
    pcb->vma[pcb->nvma].share = si;
    pcb->nvma++;
    pcb->mmap_next += 4096u;
    return 0;
}

int k_vm_syscall(MmMachine *m, KPcb *pcb, uint32_t num, uint32_t a, uint32_t b) {
    struct KDemand *d = k_demand_state(m);
    uint32_t ret = 0;
    if (d == NULL || !d->ready) {
        m->r[0] = (uint32_t)-1;
        pcb->r[0] = m->r[0];
        return 0;
    }
    if (num == K_SYS_SBRK) {
        if (do_sbrk(pcb, a, &ret) != 0) {
            m->r[0] = (uint32_t)-1;
            pcb->r[0] = m->r[0];
            return 0;
        }
        cpu_event(m, "服务 执行者=操作系统 调用=sbrk 旧=%08X 新=%08X", ret, pcb->brk);
    } else if (num == K_SYS_MMAP) {
        if (do_mmap(d, pcb, a, b, &ret) != 0) {
            m->r[0] = (uint32_t)-1;
            pcb->r[0] = m->r[0];
            return 0;
        }
        cpu_event(m, "服务 执行者=操作系统 调用=mmap 地址=%08X 长度=%u", ret, b);
    } else {
        m->r[0] = (uint32_t)-1;
        pcb->r[0] = m->r[0];
        return 0;
    }
    m->r[0] = ret;
    pcb->r[0] = ret;
    return 0;
}

int k_host_file(MmMachine *m, int fd, const void *bytes, int len) {
    struct KDemand *d = k_demand_state(m);
    if (d == NULL || fd < 0 || fd >= K_FILE_N || bytes == NULL || len < 0) {
        return -1;
    }
    if (len > 256) {
        len = 256;
    }
    memcpy(d->file[fd].bytes, bytes, (size_t)len);
    d->file[fd].len = len;
    d->file[fd].used = 1;
    return 0;
}

void k_demand_copied(MmMachine *m, uint32_t pa) {
    struct KDemand *d = k_demand_state(m);
    int at;
    if (d == NULL) {
        return;
    }
    at = frame_at(d, pa);
    if (at >= 0) {
        d->frame[at].refs++;
    }
}

int k_vm_live(const MmMachine *m, int *faults, int *replaces) {
    struct KDemand *d = k_demand_state(m);
    if (d == NULL || !d->ready) {
        return -1;
    }
    if (faults != NULL) {
        *faults = d->faults;
    }
    if (replaces != NULL) {
        *replaces = d->replaces;
    }
    return 0;
}
