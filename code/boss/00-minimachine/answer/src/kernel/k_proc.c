#include "kernel/k_i.h"

#include "cpu/cpu_intr.h"
#include "kernel/k_trap.h"
#include "kernel/k_vm.h"
#include "mmu/mmu_i.h"
#include "mmu/mmu_walk.h"

#include <stdlib.h>
#include <string.h>

static const char *st_name(int state) {
    switch (state) {
    case K_ST_NEW:
        return "new";
    case K_ST_READY:
        return "ready";
    case K_ST_RUN:
        return "running";
    case K_ST_BLOCK:
        return "blocked";
    case K_ST_TERM:
        return "terminated";
    default:
        return "?";
    }
}

int k_proc_live(const MmMachine *m) {
    return m != NULL && m->kworld != NULL;
}

int k_current(const MmMachine *m) {
    if (!k_proc_live(m)) {
        return -1;
    }
    return m->kworld->current;
}

int k_switches(const MmMachine *m) {
    return k_proc_live(m) ? m->kworld->nswitch : 0;
}

int k_slices(const MmMachine *m) {
    return k_proc_live(m) ? m->kworld->nslices : 0;
}

int k_pcb(const MmMachine *m, int pid, int *state, uint32_t *ptbr) {
    const KPcb *pcb;
    if (!k_proc_live(m) || pid < 0 || pid >= K_MAX || !m->kworld->pcb[pid].used) {
        return -1;
    }
    pcb = &m->kworld->pcb[pid];
    if (state != NULL) {
        *state = pcb->state;
    }
    if (ptbr != NULL) {
        *ptbr = pcb->ptbr;
    }
    return 0;
}

int k_alloc_pid(KWorld *w) {
    int i;
    for (i = 1; i < K_MAX; i++) {
        if (!w->pcb[i].used) {
            return i;
        }
    }
    return -1;
}

int k_busy(const KWorld *w) {
    int i;
    for (i = 1; i < K_MAX; i++) {
        int state = w->pcb[i].state;
        if (!w->pcb[i].used) {
            continue;
        }
        if (state == K_ST_NEW || state == K_ST_READY || state == K_ST_RUN || state == K_ST_BLOCK) {
            return 1;
        }
    }
    return 0;
}

int k_ready_put(KWorld *w, int pid) {
    KPcb *pcb;
    if (pid <= 0 || pid >= K_MAX) {
        return -1;
    }
    pcb = &w->pcb[pid];
    if (!pcb->used || pcb->in_queue) {
        return pcb->in_queue ? 0 : -1;
    }
    if (pcb->state != K_ST_READY && k_trans_ok(st_name(pcb->state), "ready", NULL) != 1) {
        return -1;
    }
    if (!cyc_enq(&w->ready, pid)) {
        return -1;
    }
    pcb->in_queue = 1;
    pcb->state = K_ST_READY;
    return 0;
}

void k_snap(const MmMachine *m, KPcb *pcb, int from_epc) {
    int i;
    for (i = 0; i < 8; i++) {
        pcb->r[i] = m->r[i];
    }
    if (from_epc) {
        pcb->pc = m->sr[MM_SR_EPC];
        pcb->psw = m->sr[MM_SR_EPSW] & 0xffu;
    } else {
        pcb->pc = m->pc;
        pcb->psw = m->psw & 0xffu;
    }
}

void k_install(MmMachine *m, KPcb *pcb, int iret) {
    int i;
    for (i = 0; i < 8; i++) {
        m->r[i] = pcb->r[i];
    }
    if (m->mmu != NULL) {
        m->mmu->ptbr = pcb->ptbr;
    }
    m->sr[MM_SR_PTBR] = pcb->ptbr;
    mmu_flush(m);
    if (pcb->need_store && pcb->wait_ptr != 0) {
        mmu_mem_write(m, pcb->wait_ptr, 4, (uint32_t)pcb->wait_code);
        pcb->need_store = 0;
    }
    if (iret) {
        m->sr[MM_SR_EPC] = pcb->pc;
        m->sr[MM_SR_EPSW] = pcb->psw & 0xffu;
        return;
    }
    m->pc = pcb->pc;
    m->psw = pcb->psw & 0xffu;
}

void k_wake_all(MmMachine *m) {
    KWorld *w = m->kworld;
    int pid = 0;
    if (w == NULL) {
        return;
    }
    w->now = m->cycles;
    while (k_timer_due(w, m->cycles, &pid)) {
        if (pid > 0 && pid < K_MAX && w->pcb[pid].used && w->pcb[pid].state == K_ST_BLOCK) {
            k_ready_put(w, pid);
        }
    }
}

static int better_live(const KWorld *w, int a, int b) {
    const KPcb *pa = &w->pcb[a];
    const KPcb *pb = &w->pcb[b];
    if (w->policy == K_SJF && pa->service > 0 && pb->service > 0 && pa->service != pb->service) {
        return pa->service < pb->service;
    }
    if (w->policy == K_SRTN && pa->remain != pb->remain) {
        return pa->remain < pb->remain;
    }
    if ((w->policy == K_PRIO_NP || w->policy == K_PRIO_P) && pa->prio != pb->prio) {
        return w->large ? pa->prio > pb->prio : pa->prio < pb->prio;
    }
    if (w->policy == K_HRRN && pa->service > 0 && pb->service > 0) {
        long long wa = (long long)w->now - pa->born;
        long long wb = (long long)w->now - pb->born;
        long long left = (wa + pa->service) * pb->service;
        long long right = (wb + pb->service) * pa->service;
        if (left != right) {
            return left > right;
        }
    }
    return a < b;
}

static int pick_front(KWorld *w) {
    int pid = 0;
    if (!cyc_deq(&w->ready, &pid)) {
        return -1;
    }
    w->pcb[pid].in_queue = 0;
    w->pcb[pid].state = K_ST_RUN;
    return pid;
}

static int pick_best(KWorld *w) {
    int ids[K_MAX];
    int n = 0;
    int x = 0;
    int i;
    int best = 0;
    while (cyc_deq(&w->ready, &x)) {
        w->pcb[x].in_queue = 0;
        ids[n++] = x;
    }
    if (n == 0) {
        return -1;
    }
    for (i = 1; i < n; i++) {
        if (better_live(w, ids[i], ids[best])) {
            best = i;
        }
    }
    for (i = 0; i < n; i++) {
        if (i != best) {
            w->pcb[ids[i]].state = K_ST_READY;
            k_ready_put(w, ids[i]);
        }
    }
    w->pcb[ids[best]].state = K_ST_RUN;
    return ids[best];
}

static KPick pick_of(int algo) {
    if (algo == K_FCFS || algo == K_RR || algo == K_MLFQ || algo == K_FB2) {
        return pick_front;
    }
    return pick_best;
}

int k_queue_beats(KWorld *w, int current) {
    int ids[K_MAX];
    int n = 0;
    int x = 0;
    int i;
    int hit = 0;
    while (cyc_deq(&w->ready, &x)) {
        w->pcb[x].in_queue = 0;
        ids[n++] = x;
    }
    for (i = 0; i < n; i++) {
        if (w->policy == K_SRTN && w->pcb[ids[i]].remain < w->pcb[current].remain) {
            hit = 1;
        }
        if (w->policy == K_PRIO_P && w->pcb[ids[i]].prio != w->pcb[current].prio) {
            int higher = w->large ? w->pcb[ids[i]].prio > w->pcb[current].prio
                                  : w->pcb[ids[i]].prio < w->pcb[current].prio;
            if (higher) {
                hit = 1;
            }
        }
        if (w->policy == K_MLFQ || w->policy == K_FB2) {
            hit = 1;
        }
        w->pcb[ids[i]].state = K_ST_READY;
        k_ready_put(w, ids[i]);
    }
    return hit;
}

int k_schedule(MmMachine *m, int iret) {
    KWorld *w = m->kworld;
    int cur = w->current;
    int next;
    w->now = m->cycles;
    k_wake_all(m);
    next = w->pick(w);
    if (next < 0) {
        if (!k_busy(w)) {
            m->exited = 1;
            m->exit_code = w->pcb[cur].exit_code;
            m->halted = 1;
            m->asleep = 0;
            return 1;
        }
        next = 0;
        w->pcb[0].state = K_ST_RUN;
    }
    if (next != cur) {
        w->nswitch++;
        cpu_event(m, "切换 from=%d to=%d 保存=R0-R7,PC,PSW,PTBR TLBFLUSH", cur, next);
    }
    w->current = next;
    k_install(m, &w->pcb[next], iret);
    return 0;
}

static void k_on_step(MmMachine *m) {
    KWorld *w;
    int cur;
    if (m->kworld == NULL || m->halted) {
        return;
    }
    w = m->kworld;
    w->now = m->cycles;
    k_wake_all(m);
    cur = w->current;
    if (cur == 0 && !cyc_empty(&w->ready)) {
        k_snap(m, &w->pcb[0], 0);
        w->pcb[0].state = K_ST_READY;
        k_schedule(m, 0);
        return;
    }
    if (cur > 0 && k_when(w->policy, K_WHEN_ARRIVE) && k_queue_beats(w, cur)) {
        k_snap(m, &w->pcb[cur], 0);
        k_ready_put(w, cur);
        k_schedule(m, 0);
    }
}

int k_proc_timer(MmMachine *m) {
    KWorld *w = m->kworld;
    int cur;
    cpu_eoi(m, 0);
    if (w == NULL || m->pipe_on) {
        return 0;
    }
    w->nslices++;
    w->now = m->cycles;
    k_wake_all(m);
    cur = w->current;
    if (cur > 0 && w->pcb[cur].state == K_ST_RUN && k_when(w->policy, K_WHEN_SLICE)) {
        k_snap(m, &w->pcb[cur], 1);
        if (k_ready_put(w, cur) != 0) {
            return 0;
        }
        return k_schedule(m, 1);
    }
    if (cur == 0 && !cyc_empty(&w->ready)) {
        k_snap(m, &w->pcb[0], 1);
        w->pcb[0].state = K_ST_READY;
        return k_schedule(m, 1);
    }
    return 0;
}

int k_proc_fault(MmMachine *m, int vec) {
    KWorld *w;
    KPcb *pcb;
    if (!k_proc_live(m)) {
        return 1;
    }
    w = m->kworld;
    if (w->current <= 0) {
        m->halted = 1;
        return 1;
    }
    pcb = &w->pcb[w->current];
    pcb->exit_code = vec;
    pcb->state = K_ST_TERM;
    cpu_event(m, "故障 进程=%d 向量=%d 结果=结束进程", pcb->pid, vec);
    return k_schedule(m, 1);
}

int k_proc_policy(MmMachine *m, int algo, int quantum, int large) {
    KWorld *w;
    if (!k_proc_live(m) || algo < 0 || algo >= K_ALGO_N || quantum < 0 || quantum > 1000000) {
        return -1;
    }
    if ((algo == K_PRIO_NP || algo == K_PRIO_P) && large != 0 && large != 1) {
        return -1;
    }
    w = m->kworld;
    w->policy = algo;
    w->quantum = quantum;
    w->large = large ? 1 : 0;
    w->pick = pick_of(algo);
    if (k_when(algo, K_WHEN_SLICE) && quantum > 0) {
        cpu_timer_set(m, (uint32_t)quantum);
    } else {
        cpu_timer_set(m, 0);
    }
    return 0;
}

void k_proc_drop(MmMachine *m) {
    KWorld *w;
    int i;
    int j;
    if (m == NULL) {
        return;
    }
    k_demand_off(m);
    if (m->kworld == NULL) {
        return;
    }
    w = m->kworld;
    for (i = 0; i < w->nimage; i++) {
        for (j = 0; j < w->image[i].npages; j++) {
            free(w->image[i].bytes[j]);
        }
    }
    free(w);
    m->kworld = NULL;
    m->k_step = NULL;
}

int k_proc_on(MmMachine *m) {
    KWorld *w;
    if (m == NULL || m->mem_size < 0x000E0000u) {
        return -1;
    }
    if (m->kworld != NULL) {
        return 0;
    }
    if (m->handler[MM_EX_SYSCALL] == NULL && k_boot(m) != 0) {
        return -1;
    }
    w = calloc(1, sizeof *w);
    if (w == NULL) {
        return -1;
    }
    if (!cyc_init(&w->ready, 8)) {
        free(w);
        return -1;
    }
    heap_init(&w->sleepers, 0);
    w->policy = K_FCFS;
    w->large = 1;
    w->pick = pick_front;
    if (k_as_setup(m, w) != 0) {
        free(w);
        return -1;
    }
    m->kworld = w;
    m->k_step = k_on_step;
    m->pipe_on = 0;
    return 0;
}
