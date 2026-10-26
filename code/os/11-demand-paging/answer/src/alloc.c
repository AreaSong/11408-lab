#include "alloc.h"

#include <string.h>

/* 2015-30：固定分配和全局置换不能组合。全局置换会改变进程的块数，固定分配要求块数不变。
 * 淘汰用 FIFO，只为把三种分配策略的缺页数分开。OPT、LRU、CLOCK 在第 12 关。 */

int alloc_named(const char *name) {
    if (strcmp(name, "fixed-local") == 0) {
        return ALLOC_FIXED_LOCAL;
    }
    if (strcmp(name, "var-global") == 0) {
        return ALLOC_VAR_GLOBAL;
    }
    if (strcmp(name, "var-local") == 0) {
        return ALLOC_VAR_LOCAL;
    }
    if (strcmp(name, "fixed-global") == 0) {
        return -2;
    }
    return -1;
}

int alloc_combo(const char *name, int *legal) {
    int id = alloc_named(name);

    if (id == -1) {
        return -1;
    }
    *legal = id > 0;
    return 0;
}

const char *alloc_policy_name(int policy) {
    if (policy == ALLOC_FIXED_LOCAL) {
        return "fixed-local";
    }
    if (policy == ALLOC_VAR_GLOBAL) {
        return "var-global";
    }
    if (policy == ALLOC_VAR_LOCAL) {
        return "var-local";
    }
    return "unknown";
}

int alloc_init(Alloc *a, int policy, int frames, int nproc, const int *base) {
    int i;
    int sum = 0;

    if (frames < 1 || frames > ALLOC_SLOTS || nproc < 1 || nproc > ALLOC_PROC) {
        return -1;
    }
    if (policy != ALLOC_FIXED_LOCAL && policy != ALLOC_VAR_GLOBAL && policy != ALLOC_VAR_LOCAL) {
        return -1;
    }
    for (i = 0; i < nproc; i++) {
        if (base[i] < 1) {
            return -1;
        }
        sum += base[i];
    }
    if (sum > frames) {
        return -1;
    }
    memset(a, 0, sizeof *a);
    a->ready = 1;
    a->policy = policy;
    a->frames = frames;
    a->nproc = nproc;
    a->pool = frames - sum;
    for (i = 0; i < nproc; i++) {
        a->proc[i].cap = base[i];
    }
    return 0;
}

static int holds(const Alloc *a, int pid, int page) {
    int i;

    for (i = 0; i < a->frames; i++) {
        if (a->slot[i].used && a->slot[i].pid == pid && a->slot[i].page == page) {
            return 1;
        }
    }
    return 0;
}

static int count_pid(const Alloc *a, int pid) {
    int i;
    int n = 0;

    for (i = 0; i < a->frames; i++) {
        if (a->slot[i].used && a->slot[i].pid == pid) {
            n++;
        }
    }
    return n;
}

static int free_slot(const Alloc *a) {
    int i;

    for (i = 0; i < a->frames; i++) {
        if (!a->slot[i].used) {
            return i;
        }
    }
    return -1;
}

/* pid < 0 表示全局：所有进程的页都可淘汰。局部只看本进程。 */
static int fifo_slot(const Alloc *a, int pid) {
    int i;
    int best = -1;
    unsigned seq = 0;

    for (i = 0; i < a->frames; i++) {
        if (!a->slot[i].used) {
            continue;
        }
        if (pid >= 0 && a->slot[i].pid != pid) {
            continue;
        }
        if (best < 0 || a->slot[i].seq < seq) {
            best = i;
            seq = a->slot[i].seq;
        }
    }
    return best;
}

static void occupy(Alloc *a, int slot, int pid, int page) {
    a->slot[slot].used = 1;
    a->slot[slot].pid = pid;
    a->slot[slot].page = page;
    a->slot[slot].seq = ++a->seq;
}

static void fill_ref(const Alloc *a, int pid, int hit, int grew, int vpid, int vpage, AllocRef *out) {
    out->hit = hit;
    out->grew = grew;
    out->victim_pid = vpid;
    out->victim_page = vpage;
    out->cap = a->proc[pid].cap;
    out->pool = a->pool;
    out->faults = a->proc[pid].faults;
    out->replaces = a->proc[pid].replaces;
}

static int take_free(Alloc *a, int pid, int page, int grew, AllocRef *out) {
    int slot = free_slot(a);

    if (slot < 0) {
        return -1;
    }
    occupy(a, slot, pid, page);
    fill_ref(a, pid, 0, grew, -1, -1, out);
    return 0;
}

static int replace_ref(Alloc *a, int pid, int page, AllocRef *out) {
    int only = a->policy == ALLOC_VAR_GLOBAL ? -1 : pid;
    int slot = fifo_slot(a, only);
    int vpid;
    int vpage;

    if (slot < 0) {
        return -1;
    }
    vpid = a->slot[slot].pid;
    vpage = a->slot[slot].page;
    if (vpid != pid) {
        a->proc[vpid].cap--;
        a->proc[pid].cap++;
    }
    a->proc[pid].replaces++;
    occupy(a, slot, pid, page);
    fill_ref(a, pid, 0, 0, vpid, vpage, out);
    return 0;
}

int alloc_ref(Alloc *a, int pid, int page, AllocRef *out) {
    if (!a->ready || pid < 0 || pid >= a->nproc || page < 0) {
        return -1;
    }
    if (holds(a, pid, page)) {
        fill_ref(a, pid, 1, 0, -1, -1, out);
        return 0;
    }
    a->proc[pid].faults++;
    if (count_pid(a, pid) < a->proc[pid].cap) {
        return take_free(a, pid, page, 0, out);
    }
    /* 可变分配：还有空闲页框就加一块。固定分配把多出来的页框闲置。 */
    if (a->policy != ALLOC_FIXED_LOCAL && a->pool > 0) {
        a->proc[pid].cap++;
        a->pool--;
        return take_free(a, pid, page, 1, out);
    }
    return replace_ref(a, pid, page, out);
}
