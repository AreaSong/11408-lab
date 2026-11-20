#include "cpu/cpu_intr.h"

#include "common/mm_internal.h"

void cpu_set_imr(MmMachine *m, uint32_t imr) { m->sr[MM_SR_IMR] = imr & 0xffffu; }

uint32_t cpu_imr(const MmMachine *m) { return m->sr[MM_SR_IMR] & 0xffffu; }

void cpu_raise_irq(MmMachine *m, int irq) {
    if (irq >= 0 && irq < MM_IRQ_COUNT) {
        m->irq_pending |= (uint16_t)(1u << irq);
    }
}

void cpu_raise_nmi(MmMachine *m) { m->nmi_pending = 1; }

int cpu_irq_pending(const MmMachine *m, int irq) {
    if (irq < 0 || irq >= MM_IRQ_COUNT) {
        return 0;
    }
    return (m->irq_pending & (uint16_t)(1u << irq)) != 0;
}

void cpu_eoi(MmMachine *m, int irq) {
    if (irq < 0) {
        m->nmi_service = 0;
        m->nmi_pending = 0;
        return;
    }
    if (irq >= MM_IRQ_COUNT) {
        return;
    }
    m->irq_service &= (uint16_t)~(1u << irq);
    m->irq_pending &= (uint16_t)~(1u << irq);
}

static void accept_irq(MmMachine *m, int irq) {
    m->irq_pending &= (uint16_t)~(1u << irq);
    m->irq_service |= (uint16_t)(1u << irq);
    m->asleep = 0;
    m->halted = 0;
}

/* 返回 16+irq，NMI 返回 MM_EX_NMI，没有请求返回 -1。编号小的先响应。 */
static int pick_request(const MmMachine *m) {
    int i;
    if (m->nmi_pending && !m->nmi_service) {
        return MM_EX_NMI;
    }
    if ((m->psw & MM_PSW_IF) == 0 || m->deliver_depth > 16) {
        return -1;
    }
    for (i = 0; i < MM_IRQ_COUNT; i++) {
        uint16_t bit = (uint16_t)(1u << i);
        if ((m->irq_pending & bit) != 0 && (m->irq_service & bit) == 0 && (cpu_imr(m) & bit) == 0) {
            return 16 + i;
        }
    }
    return -1;
}

int cpu_poll(MmMachine *m) {
    int vec = pick_request(m);
    if (vec < 0) {
        return 0;
    }
    if (vec == MM_EX_NMI) {
        m->nmi_pending = 0;
        m->nmi_service = 1;
        m->asleep = 0;
        m->halted = 0;
    } else {
        accept_irq(m, vec - 16);
    }
    cpu_deliver(m, vec, 1);
    return 1;
}

void cpu_timer_set(MmMachine *m, uint32_t period) {
    m->port[0x11] = period;
    m->port[0x10] = period == 0 ? 0 : 1;
    m->timer_acc = 0;
}

void cpu_timer_edge(MmMachine *m) {
    uint32_t period = m->port[0x11];
    uint32_t used = m->cycles - m->step_base;
    if (m->port[0x10] == 0 || period == 0) {
        return;
    }
    m->timer_acc += used;
    while (m->timer_acc >= period) {
        m->timer_acc -= period;
        cpu_raise_irq(m, 0);
    }
}
