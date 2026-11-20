#include "cpu/cpu_intr.h"

#include "common/mm_internal.h"
#include "cpu/cpu_tick.h"

#include <stdio.h>

static void cycle_line(MmMachine *m, const char *func, const char *sig, const char *detail) {
    snprintf(m->t_stage, sizeof m->t_stage, "%s", "中断");
    snprintf(m->t_func, sizeof m->t_func, "%s", func);
    snprintf(m->t_sig, sizeof m->t_sig, "%s", sig);
    snprintf(m->t_detail, sizeof m->t_detail, "%s", detail);
    cpu_emit_tick(m);
}

static void note_mode(MmMachine *m, int old_u, int new_u, const char *why) {
    if (old_u == new_u) {
        return;
    }
    cpu_event(m, "模式 %s->%s 执行者=硬件 %s", old_u ? "用户态" : "内核态", new_u ? "用户态" : "内核态",
              why);
}

static int legacy_stop(MmMachine *m, int vec, int trap) {
    if (!trap) {
        m->pc = m->ipc;
    }
    m->halted = 1;
    m->asleep = 0;
    m->trap = trap;
    m->cause = vec;
    if (m->trace != NULL && vec >= 0) {
        fprintf(m->trace, "异常 %s 向量=%d 断点=%s %08X\n", mm_cause_name(vec), vec, trap ? "下一条" : "本条",
                trap ? m->pc : m->ipc);
    }
    return 1;
}

static int hidden_cycle(MmMachine *m, int vec, int trap, uint32_t *entry) {
    uint32_t vbr = m->sr[MM_SR_VBR];
    uint32_t addr = vbr + (uint32_t)vec * 4u;
    uint32_t word = 0;
    uint32_t epc = trap ? m->pc : m->ipc;
    uint32_t old = m->psw;
    char detail[80];
    int old_u = (old & MM_PSW_U) != 0;
    int rc = mm_raw_read(m, addr, 4, &word);
    if (rc != 0) {
        m->cause = MM_EX_MCHK;
        m->halted = 1;
        return -1;
    }
    if (word == 0) {
        return 0;
    }
    m->t_index = 0;
    m->sr[MM_SR_EPC] = epc;
    snprintf(detail, sizeof detail, "EPC=%08X", epc);
    cycle_line(m, trap ? "PC->EPC" : "IPC->EPC", trap ? "PCout EPCin" : "IPCout EPCin", detail);
    m->sr[MM_SR_EPSW] = old & 0xffu;
    m->psw = old & (uint32_t)~(MM_PSW_IF | MM_PSW_U | MM_PSW_TF);
    m->psw &= 0xffu;
    snprintf(detail, sizeof detail, "EPSW=%08X PSW=%08X", old & 0xffu, m->psw);
    cycle_line(m, "PSW->EPSW 关中断进内核态", "PSWout EPSWin", detail);
    m->z = addr;
    m->mar = addr;
    snprintf(detail, sizeof detail, "MAR=%08X", addr);
    cycle_line(m, "向量地址->MAR", "Zout MARin", detail);
    m->mdr = word;
    snprintf(detail, sizeof detail, "MDR=%08X", word);
    cycle_line(m, "M(MAR)->MDR", "MemR MDRinE", detail);
    m->pc = word;
    m->sr[MM_SR_CAUSE] = (uint32_t)vec;
    m->cause = vec;
    if (vec == MM_EX_PAGE || vec == MM_EX_PROT || vec == MM_EX_ALIGN) {
        m->sr[MM_SR_BADVA] = m->badva;
    }
    snprintf(detail, sizeof detail, "PC=%08X", m->pc);
    cycle_line(m, "MDR->PC", "MDRout PCin", detail);
    note_mode(m, old_u, 0, vec == MM_EX_SYSCALL ? "原因=系统调用" : "原因=异常或中断");
    *entry = word;
    return 1;
}

static void say_request(MmMachine *m, int vec, int trap) {
    CpuEventInfo info;
    cpu_event_info(vec, &info);
    if (vec == MM_EX_SYSCALL) {
        cpu_event(m, "SYSCALL 执行者=硬件 顺序=2 向量=8 断点=下一条 EPC=%08X", m->sr[MM_SR_EPC]);
        return;
    }
    if (vec >= 16) {
        static const char *dev[] = {"时钟", "磁盘", "网卡", "终端", "打印机", "固态盘"};
        int irq = vec - 16;
        const char *name = irq < 6 ? dev[irq] : "外部设备";
        cpu_event(m,
                  "响应 IRQ%d 名称=%s 执行者=硬件 分类=外中断 断点=下一条 能否屏蔽=是 何时响应=指令结束 EPC=%08X",
                  irq, name, m->sr[MM_SR_EPC]);
        return;
    }
    if (vec == MM_EX_NMI) {
        cpu_event(m, "响应 NMI 执行者=硬件 分类=外中断 断点=下一条 能否屏蔽=否 何时响应=指令结束 EPC=%08X",
                  m->sr[MM_SR_EPC]);
        return;
    }
    cpu_event(m, "异常 名称=%s 分类=%s 断点=%s 能否屏蔽=%s 何时响应=%s 执行者=硬件 EPC=%08X",
              mm_cause_name(vec), info.klass, trap ? "下一条" : info.epc, info.maskable ? "是" : "否", info.when,
              m->sr[MM_SR_EPC]);
}

static void hw_iret(MmMachine *m) {
    uint32_t pc = m->sr[MM_SR_EPC];
    uint32_t psw = m->sr[MM_SR_EPSW] & 0xffu;
    int vec = (int)m->sr[MM_SR_CAUSE];
    int old_u = (m->psw & MM_PSW_U) != 0;
    int new_u = (psw & MM_PSW_U) != 0;
    if ((pc & 3u) != 0) {
        m->cause = MM_EX_ALIGN;
        m->badva = pc;
        m->halted = 1;
        return;
    }
    m->pc = pc;
    m->psw = psw;
    m->cause = -1;
    note_mode(m, old_u, new_u, "经由=IRET");
    if (vec == MM_EX_SYSCALL) {
        cpu_event(m, "IRET 执行者=硬件 安排=操作系统 顺序=1 PC=%08X", pc);
        cpu_event(m, "系统调用顺序 3-2-4-1");
        return;
    }
    cpu_event(m, "IRET 执行者=硬件 PC=%08X", pc);
}

int cpu_deliver(MmMachine *m, int vec, int trap) {
    uint32_t entry = 0;
    int slot;
    int stopped;
    int live;
    if (vec < 0 || vec >= MM_VEC_COUNT) {
        m->halted = 1;
        return 1;
    }
    if (vec == MM_EX_SYSCALL) {
        cpu_event(m, "传参 执行者=用户程序 顺序=3 r0=%08X r1=%08X r2=%08X r3=%08X", m->r[0], m->r[1], m->r[2],
                  m->r[3]);
    }
    m->deliver_depth++;
    live = hidden_cycle(m, vec, trap, &entry);
    if (live < 0) {
        m->deliver_depth--;
        return 1;
    }
    if (live == 0) {
        m->deliver_depth--;
        return legacy_stop(m, vec, trap);
    }
    say_request(m, vec, trap);
    slot = cpu_host_slot(entry);
    if (slot < 0) {
        m->deliver_depth--;
        return 0;
    }
    if (m->handler[slot] == NULL) {
        m->halted = 1;
        m->deliver_depth--;
        cpu_event(m, "服务 执行者=操作系统 结果=无处理函数");
        return 1;
    }
    stopped = m->handler[slot](m, vec);
    if (stopped || vec == MM_EX_MCHK) {
        m->halted = 1;
        m->asleep = 0;
        m->deliver_depth--;
        return 1;
    }
    hw_iret(m);
    m->deliver_depth--;
    return m->halted ? 1 : 0;
}
