#include "kernel/k_vm.h"

#include <stdio.h>
#include <string.h>

enum { K_ALLOC_FIXED_LOCAL = 1, K_ALLOC_VAR_GLOBAL = 2, K_ALLOC_VAR_LOCAL = 3, K_ALLOC_SLOTS = 32 };

typedef struct {
    int used;
    int pid;
    int page;
    unsigned seq;
} Slot;

typedef struct {
    int cap;
    int faults;
    int replaces;
} Proc;

typedef struct {
    int policy;
    int frames;
    int pool;
    int nproc;
    unsigned seq;
    Proc proc[4];
    Slot slot[K_ALLOC_SLOTS];
} Alloc;

int k_alloc_id(const char *name) {
    if (strcmp(name, "fixed-local") == 0) {
        return K_ALLOC_FIXED_LOCAL;
    }
    if (strcmp(name, "var-global") == 0) {
        return K_ALLOC_VAR_GLOBAL;
    }
    if (strcmp(name, "var-local") == 0) {
        return K_ALLOC_VAR_LOCAL;
    }
    if (strcmp(name, "fixed-global") == 0) {
        return -2;
    }
    return -1;
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
    int n = 0;
    int i;
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

static int fifo_slot(const Alloc *a, int pid) {
    int i;
    int best = -1;
    unsigned seq = 0;
    for (i = 0; i < a->frames; i++) {
        if (!a->slot[i].used || (pid >= 0 && a->slot[i].pid != pid)) {
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

static void print_pair(FILE *out, int value) {
    if (value < 0) {
        fputc('-', out);
    } else {
        fprintf(out, "%d", value);
    }
}

static void print_ref(FILE *out, int pid, int page, int hit, int grew, int vpid, int vpage, const Alloc *a) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "ref pid=%d page=%d result=%s cap=%d pool=%d grew=%s victim=", pid, page, hit ? "hit" : "fault",
            a->proc[pid].cap, a->pool, grew ? "yes" : "no");
    print_pair(out, vpage);
    fputs(" from=", out);
    print_pair(out, vpid);
    fprintf(out, " faults=%d replaces=%d\n", a->proc[pid].faults, a->proc[pid].replaces);
}

static const char *policy_name(int policy) {
    if (policy == K_ALLOC_FIXED_LOCAL) {
        return "fixed-local";
    }
    if (policy == K_ALLOC_VAR_GLOBAL) {
        return "var-global";
    }
    if (policy == K_ALLOC_VAR_LOCAL) {
        return "var-local";
    }
    return "unknown";
}

static int alloc_one(Alloc *a, int who, int pg, FILE *out) {
    int hit = 0;
    int grew = 0;
    int vpid = -1;
    int vpage = -1;
    if (who < 0 || who >= a->nproc || pg < 0) {
        return -1;
    }
    if (holds(a, who, pg)) {
        hit = 1;
    } else {
        a->proc[who].faults++;
        if (count_pid(a, who) < a->proc[who].cap) {
            occupy(a, free_slot(a), who, pg);
        } else if (a->policy != K_ALLOC_FIXED_LOCAL && a->pool > 0) {
            a->proc[who].cap++;
            a->pool--;
            grew = 1;
            occupy(a, free_slot(a), who, pg);
        } else {
            int only = a->policy == K_ALLOC_VAR_GLOBAL ? -1 : who;
            int slot = fifo_slot(a, only);
            if (slot < 0) {
                return -1;
            }
            vpid = a->slot[slot].pid;
            vpage = a->slot[slot].page;
            if (vpid != who) {
                a->proc[vpid].cap--;
                a->proc[who].cap++;
            }
            a->proc[who].replaces++;
            occupy(a, slot, who, pg);
        }
    }
    print_ref(out, who, pg, hit, grew, vpid, vpage, a);
    return 0;
}

int k_alloc_run(const char *policy, int frames, const int *base, int nproc, const int *pid, const int *page,
                int nref, int *faults, int *replaces, FILE *out) {
    Alloc a;
    int id = k_alloc_id(policy);
    int sum = 0;
    int i;
    int total_f = 0;
    int total_r = 0;
    if (id < 0 || frames < 1 || frames > K_ALLOC_SLOTS || nproc < 1 || nproc > 4 || base == NULL || nref < 1) {
        return -1;
    }
    memset(&a, 0, sizeof a);
    for (i = 0; i < nproc; i++) {
        if (base[i] < 1) {
            return -1;
        }
        sum += base[i];
        a.proc[i].cap = base[i];
    }
    if (sum > frames) {
        return -1;
    }
    a.policy = id;
    a.frames = frames;
    a.nproc = nproc;
    a.pool = frames - sum;
    if (out != NULL) {
        fprintf(out, "alloc policy=%s frames=%d nproc=%d pool=%d\n", policy_name(id), frames, nproc, a.pool);
    }
    for (i = 0; i < nref; i++) {
        if (alloc_one(&a, pid[i], page[i], out) != 0) {
            return -1;
        }
    }
    for (i = 0; i < nproc; i++) {
        total_f += a.proc[i].faults;
        total_r += a.proc[i].replaces;
        if (out != NULL && i == 0) {
            fprintf(out, "summary policy=%s frames=%d pool=%d\n", policy_name(id), frames, a.pool);
        }
        if (out != NULL) {
            fprintf(out, "proc pid=%d faults=%d replaces=%d cap=%d\n", i, a.proc[i].faults, a.proc[i].replaces,
                    a.proc[i].cap);
        }
    }
    if (out != NULL) {
        fprintf(out, "total-faults=%d total-replaces=%d\n", total_f, total_r);
    }
    if (faults != NULL) {
        *faults = total_f;
    }
    if (replaces != NULL) {
        *replaces = total_r;
    }
    return 0;
}

int k_fetch_run(int prep, int frames, int ahead, int space, const int *refs, int nref, int *faults,
                int *prefetched) {
    int used[8];
    int page[8];
    unsigned seq[8];
    unsigned tick = 0;
    int i;
    int nfault = 0;
    int npre = 0;
    if (frames < 1 || frames > 8 || ahead < 0 || space < 1 || refs == NULL || nref < 1 || faults == NULL ||
        prefetched == NULL) {
        return -1;
    }
    memset(used, 0, sizeof used);
    for (i = 0; i < nref; i++) {
        int slot = -1;
        int j;
        int extra;
        int next;
        if (refs[i] < 0 || refs[i] >= space) {
            return -1;
        }
        for (j = 0; j < frames; j++) {
            if (used[j] && page[j] == refs[i]) {
                slot = j;
            }
        }
        if (slot >= 0) {
            continue;
        }
        nfault++;
        slot = -1;
        for (j = 0; j < frames; j++) {
            if (!used[j]) {
                slot = j;
                break;
            }
        }
        if (slot < 0) {
            unsigned oldest = 0;
            for (j = 0; j < frames; j++) {
                if (slot < 0 || seq[j] < oldest) {
                    slot = j;
                    oldest = seq[j];
                }
            }
        }
        used[slot] = 1;
        page[slot] = refs[i];
        seq[slot] = ++tick;
        if (!prep) {
            continue;
        }
        extra = 0;
        next = refs[i] + 1;
        while (extra < ahead && next < space) {
            int have = 0;
            int empty = -1;
            for (j = 0; j < frames; j++) {
                if (used[j] && page[j] == next) {
                    have = 1;
                }
                if (!used[j] && empty < 0) {
                    empty = j;
                }
            }
            if (have) {
                next++;
                continue;
            }
            if (empty < 0) {
                break;
            }
            used[empty] = 1;
            page[empty] = next;
            seq[empty] = ++tick;
            npre++;
            extra++;
            next++;
        }
    }
    *faults = nfault;
    *prefetched = npre;
    return 0;
}
