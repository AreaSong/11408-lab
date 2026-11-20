#include "cpu/cpu_intr.h"

#include "common/mm_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void fill_info(CpuEventInfo *out, const char *klass, const char *repair, const char *epc,
                      int maskable, const char *when) {
    out->klass = klass;
    out->repair = repair;
    out->epc = epc;
    out->maskable = maskable;
    out->when = when;
}

int cpu_event_info(int vec, CpuEventInfo *out) {
    if (out == NULL || vec < 0 || vec >= MM_VEC_COUNT) {
        return -1;
    }
    if (vec >= 16) {
        fill_info(out, "外中断", "不适用", "下一条", 1, "指令结束");
        return 0;
    }
    switch (vec) {
    case MM_EX_DIV:
    case MM_EX_ILL:
    case MM_EX_PRIV:
    case MM_EX_PROT:
    case MM_EX_ALIGN:
        fill_info(out, "故障", "不能修复", "本条", 0, "执行中");
        break;
    case MM_EX_PAGE:
        fill_info(out, "故障", "能修复", "本条", 0, "执行中");
        break;
    case MM_EX_BREAK:
    case MM_EX_SYSCALL:
    case MM_EX_OVF:
        fill_info(out, "陷阱", "不适用", "下一条", 0, "执行中");
        break;
    case MM_EX_NMI:
        fill_info(out, "外中断", "不适用", "下一条", 0, "指令结束");
        break;
    case MM_EX_MCHK:
        fill_info(out, "终止", "不适用", "无", 0, "执行中");
        break;
    default:
        fill_info(out, "保留", "不适用", "无", 0, "执行中");
        break;
    }
    return 0;
}

int cpu_is_privileged(int op) {
    switch (op) {
    case MM_OP_IRET:
    case MM_OP_EI:
    case MM_OP_DI:
    case MM_OP_HLT:
    case MM_OP_IN:
    case MM_OP_OUT:
    case MM_OP_MFSR:
    case MM_OP_MTSR:
    case MM_OP_TLBFLUSH:
        return 1;
    default:
        return 0;
    }
}

int cpu_is_host(uint32_t addr) {
    return addr >= MM_HOST_BASE && addr - MM_HOST_BASE < MM_HOST_SIZE;
}

uint32_t cpu_host_entry(int slot) {
    if (slot < 0 || slot >= MM_VEC_COUNT) {
        return 0;
    }
    return MM_HOST_BASE + (uint32_t)slot * 4u;
}

int cpu_host_slot(uint32_t addr) {
    uint32_t off;
    if (!cpu_is_host(addr)) {
        return -1;
    }
    off = addr - MM_HOST_BASE;
    if ((off & 3u) != 0) {
        return -1;
    }
    return (int)(off / 4u);
}

void cpu_set_event(MmMachine *m, FILE *fp) { m->event = fp; }

void cpu_event(MmMachine *m, const char *fmt, ...) {
    va_list ap;
    if (m->event == NULL) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(m->event, fmt, ap);
    va_end(ap);
    fputc('\n', m->event);
}

int cpu_set_handler(MmMachine *m, int vec, CpuHandler fn) {
    if (vec < 0 || vec >= MM_VEC_COUNT) {
        return -1;
    }
    m->handler[vec] = fn;
    return 0;
}

void cpu_intr_reset(MmMachine *m) {
    memset(m->handler, 0, sizeof m->handler);
    m->after_intr = NULL;
    m->event = NULL;
    m->irq_pending = 0;
    m->irq_service = 0;
    m->nmi_pending = 0;
    m->nmi_service = 0;
    m->asleep = 0;
    m->exited = 0;
    m->exit_code = 0;
    m->deliver_depth = 0;
    m->irq_depth = 0;
    memset(m->irq_mask, 0, sizeof m->irq_mask);
    memset(m->irq_steps, 0, sizeof m->irq_steps);
    m->timer_acc = 0;
    m->nest_len = 0;
    m->last_len = 0;
    m->con_len = 0;
    m->con_in_len = 0;
    m->con_in_pos = 0;
    m->con_out[0] = '\0';
    m->con_in[0] = '\0';
}
