#include "kernel/k_i.h"

#include "cpu/cpu_intr.h"
#include "kernel/k_trap.h"
#include "kernel/k_vm.h"
#include "mmu/mmu_walk.h"

#include <string.h>

static int fail_code(MmMachine *m, KPcb *pcb) {
    m->r[0] = (uint32_t)-1;
    pcb->r[0] = m->r[0];
    return 0;
}

static int reap(KWorld *w, int parent, int *child) {
    int i;
    for (i = 1; i < K_MAX; i++) {
        KPcb *pcb = &w->pcb[i];
        if (pcb->used && pcb->ppid == parent && pcb->state == K_ST_TERM) {
            *child = i;
            return 1;
        }
    }
    return 0;
}

static int has_child(const KWorld *w, int parent) {
    int i;
    for (i = 1; i < K_MAX; i++) {
        const KPcb *pcb = &w->pcb[i];
        if (!pcb->used || pcb->ppid != parent || pcb->state == K_ST_TERM) {
            continue;
        }
        return 1;
    }
    return 0;
}

static void wake_parent(KWorld *w, KPcb *child) {
    KPcb *parent;
    if (child->ppid <= 0 || child->ppid >= K_MAX) {
        return;
    }
    parent = &w->pcb[child->ppid];
    if (!parent->used || !parent->waiting || parent->state != K_ST_BLOCK) {
        return;
    }
    parent->r[0] = (uint32_t)child->pid;
    parent->need_store = 1;
    parent->wait_ptr = parent->r[1];
    parent->wait_code = child->exit_code;
    parent->waiting = 0;
    k_ready_put(w, parent->pid);
    child->used = 0;
}

static int sys_exit(MmMachine *m, KPcb *pcb, int code) {
    pcb->exit_code = code;
    pcb->state = K_ST_TERM;
    m->exit_code = code;
    wake_parent(m->kworld, pcb);
    cpu_event(m, "服务 执行者=操作系统 调用=exit 码=%d pid=%d 结果=进程结束", code, pcb->pid);
    return k_schedule(m, 1);
}

static int sys_fork(MmMachine *m, KPcb *pcb) {
    KWorld *w = m->kworld;
    int cid = k_alloc_pid(w);
    KPcb *child;
    if (cid < 0) {
        return fail_code(m, pcb);
    }
    child = &w->pcb[cid];
    memset(child, 0, sizeof *child);
    child->used = 1;
    child->pid = cid;
    child->ppid = pcb->pid;
    child->state = K_ST_NEW;
    child->prio = pcb->prio;
    child->service = pcb->service;
    child->remain = pcb->remain;
    child->born = m->cycles;
    if (k_as_copy(m, child, pcb) != 0) {
        child->used = 0;
        return fail_code(m, pcb);
    }
    k_snap(m, child, 1);
    child->r[0] = 0;
    m->r[0] = (uint32_t)cid;
    pcb->r[0] = m->r[0];
    if (k_ready_put(w, cid) != 0) {
        child->used = 0;
        return fail_code(m, pcb);
    }
    cpu_event(m, "服务 执行者=操作系统 调用=fork 父=%d 子=%d", pcb->pid, cid);
    if (k_when(w->policy, K_WHEN_ARRIVE) && k_queue_beats(w, pcb->pid)) {
        k_snap(m, pcb, 1);
        pcb->r[0] = (uint32_t)cid;
        if (k_ready_put(w, pcb->pid) != 0) {
            return 0;
        }
        return k_schedule(m, 1);
    }
    return 0;
}

static int read_str(MmMachine *m, uint32_t addr, char *buf, int cap) {
    int i;
    for (i = 0; i < cap - 1; i++) {
        uint32_t byte = 0;
        if (mmu_mem_read(m, addr + (uint32_t)i, 1, &byte) != 0) {
            return -1;
        }
        buf[i] = (char)byte;
        if (byte == 0) {
            return 0;
        }
    }
    buf[cap - 1] = '\0';
    return 0;
}

static int sys_exec(MmMachine *m, KPcb *pcb, uint32_t addr) {
    char name[16];
    const KImage *image;
    if (read_str(m, addr, name, (int)sizeof name) != 0) {
        return fail_code(m, pcb);
    }
    image = k_image_find(m->kworld, name);
    if (image == NULL || k_as_exec(m, pcb, image) != 0) {
        return fail_code(m, pcb);
    }
    k_install(m, pcb, 1);
    cpu_event(m, "服务 执行者=操作系统 调用=exec 进程=%d 名字=%s", pcb->pid, name);
    return 0;
}

static int sys_wait(MmMachine *m, KPcb *pcb, uint32_t ptr) {
    KWorld *w = m->kworld;
    int child = 0;
    if (reap(w, pcb->pid, &child)) {
        KPcb *kid = &w->pcb[child];
        if (ptr != 0 && mmu_mem_write(m, ptr, 4, (uint32_t)kid->exit_code) != 0) {
            return fail_code(m, pcb);
        }
        m->r[0] = (uint32_t)kid->pid;
        pcb->r[0] = m->r[0];
        kid->used = 0;
        return 0;
    }
    if (!has_child(w, pcb->pid)) {
        return fail_code(m, pcb);
    }
    k_snap(m, pcb, 1);
    pcb->r[1] = ptr;
    pcb->waiting = 1;
    pcb->state = K_ST_BLOCK;
    cpu_event(m, "服务 执行者=操作系统 调用=wait pid=%d 结果=阻塞", pcb->pid);
    return k_schedule(m, 1);
}

static int sys_sleep(MmMachine *m, KPcb *pcb, uint32_t cycles) {
    uint32_t when;
    if (cycles == 0) {
        m->r[0] = 0;
        pcb->r[0] = 0;
        return 0;
    }
    if (cycles > 0xffffffffu - m->cycles) {
        return fail_code(m, pcb);
    }
    when = m->cycles + cycles;
    if (k_timer_push(m->kworld, when, pcb->pid) != 0) {
        return fail_code(m, pcb);
    }
    k_snap(m, pcb, 1);
    pcb->r[0] = 0;
    pcb->state = K_ST_BLOCK;
    cpu_event(m, "服务 执行者=操作系统 调用=sleep pid=%d 周期=%u", pcb->pid, cycles);
    return k_schedule(m, 1);
}

int k_proc_syscall(MmMachine *m) {
    KWorld *w = m->kworld;
    KPcb *pcb;
    uint32_t num;
    uint32_t a;
    uint32_t b;
    uint32_t c;
    int ret;
    if (w == NULL || w->current < 0 || w->current >= K_MAX) {
        return 1;
    }
    pcb = &w->pcb[w->current];
    num = m->r[0];
    a = m->r[1];
    b = m->r[2];
    c = m->r[3];
    if (num == K_SYS_EXIT) {
        return sys_exit(m, pcb, (int)a);
    }
    if (num == K_SYS_FORK) {
        return sys_fork(m, pcb);
    }
    if (num == K_SYS_EXEC) {
        return sys_exec(m, pcb, a);
    }
    if (num == K_SYS_WAIT) {
        return sys_wait(m, pcb, a);
    }
    if (num == K_SYS_GETPID) {
        m->r[0] = (uint32_t)pcb->pid;
        pcb->r[0] = m->r[0];
        return 0;
    }
    if (num == K_SYS_SLEEP) {
        return sys_sleep(m, pcb, a);
    }
    if (num == K_SYS_WRITE || num == K_SYS_READ) {
        ret = k_con_io(m, num, a, b, c);
        m->r[0] = (uint32_t)ret;
        pcb->r[0] = m->r[0];
        return 0;
    }
    if (num >= K_SYS_SEM_CREATE && num <= K_SYS_SEM_FREE) {
        return k_sem_syscall(m, pcb, num, a);
    }
    if (num == K_SYS_SBRK || num == K_SYS_MMAP) {
        return k_vm_syscall(m, pcb, num, a, b);
    }
    cpu_event(m, "服务 执行者=操作系统 调用=未知 号=%u 返回=-1", num);
    return fail_code(m, pcb);
}
