#include "kernel/k_irq.h"
#include "kernel/k_proc.h"
#include "kernel/k_trap.h"

#include "common/mm_internal.h"
#include "cpu/cpu_intr.h"
#include "mmu/mmu_walk.h"

#include <string.h>

static int k_dispatch(MmMachine *m, int vec) {
    if (vec == MM_EX_SYSCALL) {
        return k_syscall(m);
    }
    if (vec == MM_EX_NMI) {
        return k_nmi(m);
    }
    if (vec >= 16) {
        return k_irq(m, vec);
    }
    if (k_proc_live(m)) {
        return k_proc_fault(m, vec);
    }
    if (vec == MM_EX_BREAK || vec == MM_EX_OVF) {
        cpu_event(m, "服务 执行者=操作系统 调用=%s 结果=返回下一条", mm_cause_name(vec));
        return 0;
    }
    if (vec == MM_EX_PAGE) {
        /* 阶段 04 先把要用的页装好。没装的页只有登记过 fix 才能补上并重执行。 */
        if (mmu_repair(m) == 0) {
            cpu_event(m, "服务 执行者=操作系统 调用=缺页 结果=装入后重新执行");
            return 0;
        }
        cpu_event(m, "服务 执行者=操作系统 调用=缺页 结果=结束进程");
        return 1;
    }
    cpu_event(m, "服务 执行者=操作系统 调用=%s 结果=结束进程", mm_cause_name(vec));
    return 1;
}

int k_boot(MmMachine *m) {
    int i;
    uint32_t vbr = mm_sr(m, MM_SR_VBR);
    for (i = 0; i < MM_VEC_COUNT; i++) {
        if (mm_raw_write(m, vbr + (uint32_t)i * 4u, 4, cpu_host_entry(i)) != 0) {
            return -1;
        }
        cpu_set_handler(m, i, k_dispatch);
    }
    mm_set_sr(m, MM_SR_KSP, 0x000E0000u);
    for (i = 0; i < MM_IRQ_COUNT; i++) {
        m->irq_mask[i] = (0xffffu << i) & 0xffffu;
        m->irq_steps[i] = 1;
    }
    m->after_intr = k_flush_order;
    return 0;
}

void k_set_irq_mask(MmMachine *m, int irq, uint32_t mask) {
    if (irq >= 0 && irq < MM_IRQ_COUNT) {
        m->irq_mask[irq] = mask & 0xffffu;
    }
}

void k_set_irq_steps(MmMachine *m, int irq, int steps) {
    if (irq >= 0 && irq < MM_IRQ_COUNT && steps > 0 && steps < 256) {
        m->irq_steps[irq] = (uint8_t)steps;
    }
}

void k_console_set_in(MmMachine *m, const char *text) {
    size_t n = text == NULL ? 0 : strlen(text);
    if (n >= sizeof m->con_in) {
        n = sizeof m->con_in - 1;
    }
    memcpy(m->con_in, text == NULL ? "" : text, n);
    m->con_in[n] = '\0';
    m->con_in_len = (int)n;
    m->con_in_pos = 0;
}

int k_console_out(const MmMachine *m, char *buf, int cap) {
    int n = m->con_len;
    if (buf == NULL || cap <= 0) {
        return n;
    }
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(buf, m->con_out, (size_t)n);
    buf[n] = '\0';
    return m->con_len;
}

int k_exited(const MmMachine *m, int *code) {
    if (!m->exited) {
        return 0;
    }
    if (code != NULL) {
        *code = m->exit_code;
    }
    return 1;
}

int k_last_order(const MmMachine *m, int *irq, int cap) {
    int n = m->last_len;
    int i;
    if (n > cap) {
        n = cap;
    }
    for (i = 0; i < n; i++) {
        irq[i] = m->last_seq[i];
    }
    return m->last_len;
}
