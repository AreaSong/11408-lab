#include "cpu/cpu_tick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_sig(MmMachine *m, const char *stage, const char *sig) {
    snprintf(m->t_stage, sizeof m->t_stage, "%s", stage);
    snprintf(m->t_sig, sizeof m->t_sig, "%s", sig);
}

static void set_func(MmMachine *m, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->t_func, sizeof m->t_func, fmt, ap);
    va_end(ap);
}

static void set_det(MmMachine *m, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m->t_detail, sizeof m->t_detail, fmt, ap);
    va_end(ap);
}

static int mark_fault(MmMachine *m, int vec, uint32_t addr) {
    m->cause = vec;
    m->badva = addr;
    return TK_FAULT;
}

int cpu_tick_fetch(MmMachine *m, int kind) {
    uint32_t data = 0;
    int vec;
    set_sig(m, "取指", "无");
    if (kind == K_FETCH_MAR) {
        m->ipc = m->pc;
        m->mar = m->pc;
        if ((m->psw & MM_PSW_U) != 0 && cpu_is_host(m->pc)) {
            set_func(m, "用户态进入宿主入口");
            set_det(m, "PC=%08X", m->pc);
            return mark_fault(m, MM_EX_PROT, m->pc);
        }
        set_sig(m, "取指", "PCout MARin IPCin");
        set_func(m, "PC->MAR PC->IPC");
        set_det(m, "PC=%08X MAR=%08X IPC=%08X", m->pc, m->mar, m->ipc);
        return TK_OK;
    }
    if (kind == K_FETCH_MEM) {
        set_sig(m, "取指", "MemR MDRinE PC+4");
        set_func(m, "M(MAR)->MDR PC+4->PC");
        m->mem_if = 1;
        vec = mm_read(m, m->mar, 4, &data);
        m->mem_if = 0;
        if (vec != 0) {
            set_det(m, "MAR=%08X 失败", m->mar);
            return mark_fault(m, vec, m->mar);
        }
        m->mdr = data;
        m->pc += 4;
        m->fetch_mem = 1;
        set_det(m, "MDR=%08X PC=%08X", m->mdr, m->pc);
        return TK_OK;
    }
    if (kind == K_FETCH_IR) {
        m->ir = m->mdr;
        set_sig(m, "取指", "MDRout IRin");
        set_func(m, "MDR->IR");
        set_det(m, "IR=%08X", m->ir);
        return TK_OK;
    }
    cpu_decode(m->ir, &m->din);
    set_sig(m, "译码", "无");
    set_func(m, "译码");
    if (m->din.illegal || cpu_disasm(m->ir, m->ipc, m->t_detail, sizeof m->t_detail) != 0) {
        set_det(m, "IR=%08X 非法", m->ir);
    }
    return TK_OK;
}

static int move_reg(MmMachine *m, uint32_t value, const char *sig, const char *func, uint32_t *dest) {
    *dest = value;
    set_sig(m, "执行", sig);
    set_func(m, "%s", func);
    return TK_OK;
}

int cpu_tick_move(MmMachine *m, int kind) {
    const CpuInsn *in = &m->din;
    char text[64];
    set_sig(m, "执行", "无");
    switch (kind) {
    case K_RS_Y:
        m->y = m->r[in->rs];
        set_sig(m, "执行", "RSout Yin");
        set_func(m, "r%d->Y", in->rs);
        set_det(m, "Y=%08X", m->y);
        return TK_OK;
    case K_RX_Y:
        m->y = m->r[in->rs];
        set_sig(m, "执行", "Rxout Yin");
        set_func(m, "r%d->Y", in->rs);
        set_det(m, "Y=%08X", m->y);
        return TK_OK;
    case K_RX_MAR:
        m->mar = m->r[in->rs];
        set_sig(m, "执行", "Rxout MARin");
        set_func(m, "r%d->MAR", in->rs);
        set_det(m, "MAR=%08X", m->mar);
        return TK_OK;
    case K_RD_MDR:
        m->mdr = m->r[in->rd];
        set_sig(m, "执行", "RDout MDRin");
        set_func(m, "r%d->MDR", in->rd);
        set_det(m, "MDR=%08X", m->mdr);
        return TK_OK;
    case K_RS_RD:
        m->r[in->rd] = m->r[in->rs];
        set_sig(m, "执行", "RSout RDin");
        set_func(m, "r%d->r%d", in->rs, in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_RS_PC:
        if ((m->r[in->rs] & 3u) != 0) {
            set_func(m, "r%d->PC 未对齐", in->rs);
            set_det(m, "目标=%08X", m->r[in->rs]);
            return mark_fault(m, MM_EX_ALIGN, m->r[in->rs]);
        }
        if ((m->psw & MM_PSW_U) != 0 && cpu_is_host(m->r[in->rs])) {
            set_func(m, "用户态不能进入宿主入口");
            set_det(m, "目标=%08X", m->r[in->rs]);
            return mark_fault(m, MM_EX_PROT, m->r[in->rs]);
        }
        m->pc = m->r[in->rs];
        set_sig(m, "执行", "RSout PCin");
        set_func(m, "r%d->PC", in->rs);
        set_det(m, "PC=%08X", m->pc);
        return TK_OK;
    case K_PC_Y:
        m->y = m->pc;
        set_sig(m, "执行", "PCout Yin");
        set_func(m, "PC->Y");
        set_det(m, "Y=%08X", m->y);
        return TK_OK;
    case K_PC_SAVE:
        m->ret_save = m->pc;
        set_func(m, "锁存返回地址");
        set_det(m, "返回地址=%08X", m->ret_save);
        return TK_OK;
    case K_PC_SAVE_Y:
        m->ret_save = m->pc;
        m->y = m->pc;
        set_sig(m, "执行", "PCout Yin");
        set_func(m, "PC->Y 锁存返回地址");
        set_det(m, "Y=%08X 返回地址=%08X", m->y, m->ret_save);
        return TK_OK;
    case K_Z_MAR:
        m->mar = m->z;
        set_sig(m, "执行", "Zout MARin");
        set_func(m, "Z->MAR");
        set_det(m, "MAR=%08X", m->mar);
        return TK_OK;
    case K_Z_RD:
        m->r[in->rd] = m->z;
        set_sig(m, "执行", "Zout RDin");
        set_func(m, "Z->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_Z_PC:
        if ((m->z & 3u) != 0) {
            set_func(m, "Z->PC 未对齐");
            set_det(m, "目标=%08X", m->z);
            return mark_fault(m, MM_EX_ALIGN, m->z);
        }
        if ((m->psw & MM_PSW_U) != 0 && cpu_is_host(m->z)) {
            set_func(m, "用户态不能进入宿主入口");
            set_det(m, "目标=%08X", m->z);
            return mark_fault(m, MM_EX_PROT, m->z);
        }
        m->pc = m->z;
        set_sig(m, "执行", "Zout PCin");
        set_func(m, "Z->PC");
        set_det(m, "PC=%08X", m->pc);
        return TK_OK;
    case K_LK_R6:
        m->r[6] = m->ret_save;
        set_sig(m, "执行", "LKout R6in");
        set_func(m, "返回地址->r6");
        set_det(m, "r6=%08X", m->r[6]);
        return TK_OK;
    case K_SE_RD:
        m->r[in->rd] = (uint32_t)mm_sext16(in->imm);
        set_sig(m, "执行", "SEout RDin");
        set_func(m, "sext(A)->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_ZE_MAR:
        m->mar = in->imm;
        set_sig(m, "执行", "ZEout MARin");
        set_func(m, "zext(A)->MAR");
        set_det(m, "MAR=%08X", m->mar);
        return TK_OK;
    case K_ZE_RD:
        m->r[in->rd] = in->imm;
        set_sig(m, "执行", "ZEout RDin");
        set_func(m, "zext(A)->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_MDR_RD:
        m->r[in->rd] = m->mdr;
        set_sig(m, "执行", "MDRout RDin");
        set_func(m, "MDR->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_MDR_SE:
        m->r[in->rd] = (uint32_t)(int32_t)(int8_t)(m->mdr & 0xffu);
        set_sig(m, "执行", "MDRout SextB RDin");
        set_func(m, "MDR 字节符号扩展->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_MDR_ZE:
        m->r[in->rd] = m->mdr & 0xffu;
        set_sig(m, "执行", "MDRout ZextB RDin");
        set_func(m, "MDR 字节零扩展->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    case K_MDR_MAR:
        m->mar = m->mdr;
        set_sig(m, "执行", "MDRout MARin");
        set_func(m, "MDR->MAR");
        set_det(m, "MAR=%08X", m->mar);
        return TK_OK;
    case K_MDR_Z:
        m->z = m->mdr;
        set_sig(m, "执行", "MDRout Zin");
        set_func(m, "MDR->Z");
        set_det(m, "Z=%08X", m->z);
        return TK_OK;
    case K_ONE_MDR:
        m->mdr = 1;
        set_sig(m, "执行", "Oneout MDRin");
        set_func(m, "1->MDR");
        set_det(m, "MDR=%08X", m->mdr);
        return TK_OK;
    case K_LUI:
        m->r[in->rd] = (uint32_t)in->imm << 16;
        set_sig(m, "执行", "LUIout RDin");
        set_func(m, "imm<<16->r%d", in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    default:
        snprintf(text, sizeof text, "未知传送");
        return move_reg(m, 0, "无", text, &m->z);
    }
}

int cpu_tick_mem(MmMachine *m, int kind) {
    int size = (kind == K_READ_B || kind == K_WRITE_B) ? 1 : 4;
    int writing = kind == K_WRITE_W || kind == K_WRITE_B;
    uint32_t data = m->mdr;
    int vec;
    set_sig(m, "执行", writing ? "MDRoutE MemW" : "MemR MDRinE");
    set_func(m, "%s", writing ? "MDR->M(MAR)" : "M(MAR)->MDR");
    if (size == 1 && !writing) {
        set_func(m, "M(MAR)->MDR 字节");
    }
    vec = writing ? mm_write(m, m->mar, size, data) : mm_read(m, m->mar, size, &data);
    if (vec != 0) {
        set_det(m, "MAR=%08X 失败", m->mar);
        return mark_fault(m, vec, m->mar);
    }
    if (!writing) {
        m->mdr = data;
    }
    m->op_mem++;
    set_det(m, "MAR=%08X MDR=%08X", m->mar, m->mdr);
    return TK_OK;
}
