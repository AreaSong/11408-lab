#include "kernel/k_frame.h"
#include "kernel/k_trap.h"

#include "cpu/cpu_intr.h"

#include <stdio.h>

static int push_frame(MmMachine *m, uint32_t epc, uint32_t epsw, uint32_t imr) {
    int i;
    if (k_push(m, imr) != 0 || k_push(m, epsw) != 0 || k_push(m, epc) != 0) {
        return -1;
    }
    for (i = 7; i >= 0; i--) {
        if (k_push(m, m->r[i]) != 0) {
            return -1;
        }
    }
    cpu_event(m, "现场 执行者=操作系统 保存=EPC,EPSW,IMR,R0-R7 栈=%08X", mm_sr(m, MM_SR_KSP));
    return 0;
}

static int pop_frame(MmMachine *m, uint32_t *epc, uint32_t *epsw, uint32_t *imr) {
    int i;
    for (i = 0; i < 8; i++) {
        uint32_t value = 0;
        if (k_pop(m, &value) != 0) {
            return -1;
        }
        m->r[i] = value;
    }
    return k_pop(m, epc) == 0 && k_pop(m, epsw) == 0 && k_pop(m, imr) == 0 ? 0 : -1;
}

static void remember(MmMachine *m, int irq) {
    if (m->nest_len < 32) {
        m->nest_seq[m->nest_len++] = irq;
    }
    cpu_event(m, "完成 IRQ%d", irq);
}

static void run_body(MmMachine *m, int irq) {
    int steps = m->irq_steps[irq] > 0 ? m->irq_steps[irq] : 1;
    int i;
    m->psw |= MM_PSW_IF;
    cpu_event(m, "开中断 执行者=操作系统 IRQ%d", irq);
    for (i = 0; i < steps && !m->halted; i++) {
        cpu_poll(m);
    }
    m->psw &= ~MM_PSW_IF;
    cpu_event(m, "关中断 执行者=操作系统 IRQ%d", irq);
}

int k_irq(MmMachine *m, int vec) {
    int irq = vec - 16;
    uint32_t epc = mm_sr(m, MM_SR_EPC);
    uint32_t epsw = mm_sr(m, MM_SR_EPSW);
    uint32_t imr = cpu_imr(m);
    uint32_t saved_epc = 0;
    uint32_t saved_psw = 0;
    uint32_t saved_imr = 0;
    m->irq_depth++;
    if (push_frame(m, epc, epsw, imr) != 0) {
        m->irq_depth--;
        return 1;
    }
    cpu_set_imr(m, m->irq_mask[irq]);
    cpu_event(m, "屏蔽字 IMR=%08X 执行者=操作系统 IRQ%d", cpu_imr(m), irq);
    run_body(m, irq);
    if (pop_frame(m, &saved_epc, &saved_psw, &saved_imr) != 0) {
        m->irq_depth--;
        return 1;
    }
    mm_set_sr(m, MM_SR_EPC, saved_epc);
    mm_set_sr(m, MM_SR_EPSW, saved_psw);
    cpu_set_imr(m, saved_imr);
    cpu_eoi(m, irq);
    remember(m, irq);
    m->irq_depth--;
    return 0;
}

int k_nmi(MmMachine *m) {
    cpu_event(m, "服务 执行者=操作系统 调用=NMI 结果=返回下一条");
    cpu_eoi(m, -1);
    return 0;
}

void k_flush_order(MmMachine *m) {
    int i;
    char line[160];
    int pos = 0;
    if (m->nest_len <= 0 || m->irq_depth != 0) {
        return;
    }
    pos = snprintf(line, sizeof line, "次序");
    m->last_len = m->nest_len;
    for (i = 0; i < m->nest_len && pos > 0 && pos < (int)sizeof line; i++) {
        m->last_seq[i] = m->nest_seq[i];
        pos += snprintf(line + pos, sizeof line - (size_t)pos, " IRQ%d", m->nest_seq[i]);
    }
    m->nest_len = 0;
    cpu_event(m, "%s", line);
}
