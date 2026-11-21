#include "kernel/k_i.h"
#include "kernel/k_sync.h"

#include "cpu/cpu_intr.h"

#include <string.h>

int k_sem_ok(int value, int nwait) {
    if (nwait < 0) {
        return 0;
    }
    if (value >= 0) {
        return nwait == 0;
    }
    return nwait == -value;
}

int k_sem_pv(KSemLog *s, int is_v, int pid) {
    if (s == NULL || pid < 0) {
        return -1;
    }
    if (is_v) {
        int woken = -1;
        s->value++;
        if (s->value <= 0) {
            if (s->nq <= 0) {
                return -1;
            }
            woken = s->q[0];
            memmove(s->q, s->q + 1, (size_t)(s->nq - 1) * sizeof s->q[0]);
            s->nq--;
        }
        s->nwait = s->nq;
        return woken < 0 ? 0 : woken;
    }
    s->value--;
    if (s->value < 0) {
        if (s->nq >= K_SEM_SLOTS) {
            s->value++;
            return -1;
        }
        s->q[s->nq++] = pid;
        s->nwait = s->nq;
        return 1;
    }
    s->nwait = s->nq;
    return 0;
}

int k_race_run(int n, int locked) {
    int counter = 0;
    int i;
    if (n < 1 || n > 10000) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (locked) {
            counter++;
            counter++;
        } else {
            int seen = counter;
            counter = seen + 1;
            counter = seen + 1;
        }
    }
    return counter;
}

int k_kernel_if(uint32_t psw) {
    uint32_t now = psw & ~(MM_PSW_IF | MM_PSW_U | MM_PSW_TF);
    return (now & MM_PSW_IF) == 0;
}

static int sem_id(const KWorld *w, uint32_t id) {
    if (w == NULL || id >= K_SEM_SLOTS || !w->sem[id].used) {
        return -1;
    }
    return (int)id;
}

int k_sem_stat(const MmMachine *m, int id, int *value, int *nwait) {
    const KSemSlot *s;
    if (!k_proc_live(m) || sem_id(m->kworld, (uint32_t)id) < 0) {
        return -1;
    }
    s = &m->kworld->sem[id];
    if (value != NULL) {
        *value = s->value;
    }
    if (nwait != NULL) {
        *nwait = cyc_count(&s->wait);
    }
    return 0;
}

static int fail_code(MmMachine *m, KPcb *pcb) {
    m->r[0] = (uint32_t)-1;
    pcb->r[0] = m->r[0];
    return 0;
}

static int alloc_sem(KWorld *w, int init) {
    int i;
    for (i = 0; i < K_SEM_SLOTS; i++) {
        if (w->sem[i].used) {
            continue;
        }
        memset(&w->sem[i], 0, sizeof w->sem[i]);
        if (!cyc_init(&w->sem[i].wait, K_SEM_SLOTS)) {
            return -1;
        }
        w->sem[i].used = 1;
        w->sem[i].value = init;
        return i;
    }
    return -1;
}

static int sem_create(MmMachine *m, KPcb *pcb, int init) {
    int id;
    if (init < 0 || init > 1000) {
        return fail_code(m, pcb);
    }
    id = alloc_sem(m->kworld, init);
    if (id < 0) {
        return fail_code(m, pcb);
    }
    m->r[0] = (uint32_t)id;
    pcb->r[0] = m->r[0];
    cpu_event(m, "服务 执行者=操作系统 调用=sem_create 号=%d 初值=%d", id, init);
    return 0;
}

static int sem_p(MmMachine *m, KPcb *pcb, int id) {
    KSemSlot *s;
    if (sem_id(m->kworld, (uint32_t)id) < 0) {
        return fail_code(m, pcb);
    }
    s = &m->kworld->sem[id];
    s->value--;
    if (s->value < 0) {
        if (!cyc_enq(&s->wait, pcb->pid)) {
            s->value++;
            return fail_code(m, pcb);
        }
        k_snap(m, pcb, 1);
        pcb->r[0] = 0;
        pcb->state = K_ST_BLOCK;
        cpu_event(m, "服务 执行者=操作系统 调用=sem_p 号=%d 值=%d 等待=%d 结果=阻塞", id, s->value,
                  cyc_count(&s->wait));
        return k_schedule(m, 1);
    }
    m->r[0] = 0;
    pcb->r[0] = 0;
    cpu_event(m, "服务 执行者=操作系统 调用=sem_p 号=%d 值=%d 等待=%d", id, s->value, cyc_count(&s->wait));
    return 0;
}

static int sem_v(MmMachine *m, KPcb *pcb, int id) {
    KSemSlot *s;
    int woken = -1;
    if (sem_id(m->kworld, (uint32_t)id) < 0) {
        return fail_code(m, pcb);
    }
    s = &m->kworld->sem[id];
    s->value++;
    if (s->value <= 0) {
        if (!cyc_deq(&s->wait, &woken) || k_ready_put(m->kworld, woken) != 0) {
            s->value--;
            return fail_code(m, pcb);
        }
    }
    m->r[0] = 0;
    pcb->r[0] = 0;
    cpu_event(m, "服务 执行者=操作系统 调用=sem_v 号=%d 值=%d 等待=%d 唤醒=%d", id, s->value, cyc_count(&s->wait),
              woken);
    return 0;
}

static int sem_free(MmMachine *m, KPcb *pcb, int id) {
    KSemSlot *s;
    if (sem_id(m->kworld, (uint32_t)id) < 0) {
        return fail_code(m, pcb);
    }
    s = &m->kworld->sem[id];
    if (cyc_count(&s->wait) > 0) {
        return fail_code(m, pcb);
    }
    s->used = 0;
    m->r[0] = 0;
    pcb->r[0] = 0;
    cpu_event(m, "服务 执行者=操作系统 调用=sem_free 号=%d", id);
    return 0;
}

int k_sem_syscall(MmMachine *m, KPcb *pcb, uint32_t num, uint32_t arg) {
    if (num == K_SYS_SEM_CREATE) {
        return sem_create(m, pcb, (int)arg);
    }
    if (num == K_SYS_SEM_P) {
        return sem_p(m, pcb, (int)arg);
    }
    if (num == K_SYS_SEM_V) {
        return sem_v(m, pcb, (int)arg);
    }
    if (num == K_SYS_SEM_FREE) {
        return sem_free(m, pcb, (int)arg);
    }
    return fail_code(m, pcb);
}
