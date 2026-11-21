#include "cpu/cpu_tick.h"

#include "cpu/cpu_alu.h"
#include "cpu/cpu_exec.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_walk.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int mem_cycle(const MmMachine *m) {
    return strstr(m->t_sig, "MemR") != NULL || strstr(m->t_sig, "MemW") != NULL;
}

void cpu_emit_tick(MmMachine *m) {
    int drivers = cpu_bus_drivers(m->t_sig);
    int memory = mem_cycle(m);
    m->t_index++;
    /* 命中就这一拍。缺失的主存周期加在后面，不另起微操作行。 */
    m->cycles += 1u + (uint32_t)mem_cache_take_stall(m) + (uint32_t)mmu_take_stall(m);
    if (drivers > 1) {
        m->bus_bad++;
    }
    if (m->trace != NULL) {
        fprintf(m->trace, "%s T%d %s | %s | %s | 内总线驱动 %d 个\n", m->t_stage, m->t_index, m->t_func,
                m->t_detail, m->t_sig, drivers);
        if (drivers > 1) {
            fprintf(m->trace, "冲突：同一时钟周期只允许一个部件向内总线输出\n");
        }
    }
    /* 周期挪用发生在这个存取周期结束之后。没装 DMA 时指针是空的，trace 不变。 */
    if (memory && m->after_mem != NULL) {
        m->after_mem(m);
    }
}

static void set_sig(MmMachine *m, const char *sig) {
    snprintf(m->t_stage, sizeof m->t_stage, "%s", "执行");
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

static void apply_flags(MmMachine *m, CpuAlu alu) {
    uint32_t psw = m->psw & ~0x0fu;
    if (alu.zf) {
        psw |= MM_PSW_ZF;
    }
    if (alu.sf) {
        psw |= MM_PSW_SF;
    }
    if (alu.cf) {
        psw |= MM_PSW_CF;
    }
    if (alu.of) {
        psw |= MM_PSW_OF;
    }
    m->psw = psw & 0xffu;
    m->z = alu.result;
}

static void show_flags(MmMachine *m) {
    uint32_t psw = m->psw;
    set_det(m, "Z=%08X ZF=%d SF=%d CF=%d OF=%d", m->z, (psw & MM_PSW_ZF) != 0,
            (psw & MM_PSW_SF) != 0, (psw & MM_PSW_CF) != 0, (psw & MM_PSW_OF) != 0);
}

static int mark_fault(MmMachine *m, int vec, uint32_t addr) {
    m->cause = vec;
    m->badva = addr;
    return TK_FAULT;
}

static int tick_alu_rt(MmMachine *m) {
    const CpuInsn *in = &m->din;
    uint32_t rt = m->r[in->rt];
    CpuAlu alu;
    const char *sig = "RTout Add Zin PSWup";
    switch (in->funct) {
    case MM_FN_ADD:
        alu = cpu_alu_add(m->y, rt);
        set_func(m, "Y+r%d->Z 更新PSW", in->rt);
        break;
    case MM_FN_SUB:
    case MM_FN_CMP:
        alu = cpu_alu_sub(m->y, rt);
        sig = "RTout Sub Zin PSWup";
        set_func(m, in->funct == MM_FN_CMP ? "Y-r%d->Z 不写回" : "Y-r%d->Z 更新PSW", in->rt);
        break;
    case MM_FN_AND:
        alu = cpu_alu_logic(m->y & rt);
        sig = "RTout And Zin PSWup";
        set_func(m, "Y与r%d按位与->Z", in->rt);
        break;
    case MM_FN_OR:
        alu = cpu_alu_logic(m->y | rt);
        sig = "RTout Or Zin PSWup";
        set_func(m, "Y或r%d->Z", in->rt);
        break;
    default:
        alu = cpu_alu_logic(m->y ^ rt);
        sig = "RTout Xor Zin PSWup";
        set_func(m, "Y异或r%d->Z", in->rt);
        break;
    }
    apply_flags(m, alu);
    set_sig(m, sig);
    show_flags(m);
    return TK_OK;
}

static int tick_alu_imm(MmMachine *m, int zero_ext) {
    const CpuInsn *in = &m->din;
    uint32_t rhs = zero_ext ? in->imm : (uint32_t)mm_sext16(in->imm);
    CpuAlu alu;
    const char *sig = "SEout Add Zin PSWup";
    const char *op = "sext+Y->Z 更新PSW";
    if (!zero_ext && (in->op == MM_OP_SUBI || in->op == MM_OP_CMPI)) {
        alu = cpu_alu_sub(m->y, rhs);
        sig = "SEout Sub Zin PSWup";
        op = in->op == MM_OP_CMPI ? "Y-sext->Z 不写回" : "Y-sext->Z 更新PSW";
    } else if (!zero_ext) {
        alu = cpu_alu_add(m->y, rhs);
    } else if (in->op == MM_OP_ANDI) {
        alu = cpu_alu_logic(m->y & rhs);
        sig = "ZEout And Zin PSWup";
        op = "zext 与 Y 按位与->Z";
    } else if (in->op == MM_OP_ORI) {
        alu = cpu_alu_logic(m->y | rhs);
        sig = "ZEout Or Zin PSWup";
        op = "zext 或 Y->Z";
    } else {
        alu = cpu_alu_logic(m->y ^ rhs);
        sig = "ZEout Xor Zin PSWup";
        op = "zext 异或 Y->Z";
    }
    apply_flags(m, alu);
    set_sig(m, sig);
    set_func(m, "%s", op);
    show_flags(m);
    return TK_OK;
}

static int tick_shift(MmMachine *m) {
    int kind = CPU_SHL;
    const char *sig = "RSout Shl Zin PSWup";
    int old_cf = (m->psw & MM_PSW_CF) != 0;
    if (m->din.funct == MM_FN_SHR) {
        kind = CPU_SHR;
        sig = "RSout Shr Zin PSWup";
    } else if (m->din.funct == MM_FN_SAR) {
        kind = CPU_SAR;
        sig = "RSout Sar Zin PSWup";
    }
    apply_flags(m, cpu_alu_shift(m->r[m->din.rs], m->din.shamt, kind, old_cf));
    set_sig(m, sig);
    set_func(m, "r%d 移位 #%d->Z", m->din.rs, m->din.shamt);
    show_flags(m);
    return TK_OK;
}

static int tick_div(MmMachine *m, int is_mod) {
    uint32_t result = 0;
    if (cpu_alu_div(m->y, m->r[m->din.rt], is_mod, &result) != 0) {
        set_sig(m, "RTout Div Zin");
        set_func(m, "除法错，不写回");
        set_det(m, "除数=r%d", m->din.rt);
        return mark_fault(m, MM_EX_DIV, m->ipc);
    }
    m->z = result;
    set_sig(m, is_mod ? "RTout Mod Zin" : "RTout Div Zin");
    set_func(m, is_mod ? "Y%%r%d->Z" : "Y/r%d->Z", m->din.rt);
    set_det(m, "Z=%08X", m->z);
    return TK_OK;
}

static int tick_alu(MmMachine *m, int kind) {
    CpuAlu alu;
    if (kind == K_ALU_RT) {
        return tick_alu_rt(m);
    }
    if (kind == K_ALU_SE || kind == K_ALU_ZE) {
        return tick_alu_imm(m, kind == K_ALU_ZE);
    }
    if (kind == K_ADDR_SE || kind == K_ADDR_ZE) {
        uint32_t rhs = kind == K_ADDR_ZE ? m->din.imm : (uint32_t)mm_sext16(m->din.imm);
        m->z = m->y + rhs;
        set_sig(m, kind == K_ADDR_ZE ? "ZEout Add Zin" : "SEout Add Zin");
        set_func(m, kind == K_ADDR_ZE ? "zext(A)+Y->Z" : "sext(A)+Y->Z");
        set_det(m, "Z=%08X", m->z);
        return TK_OK;
    }
    if (kind == K_NOT) {
        alu = cpu_alu_logic(~m->r[m->din.rs]);
        apply_flags(m, alu);
        set_sig(m, "RSout Not Zin PSWup");
        set_func(m, "r%d 按位取反->Z", m->din.rs);
        show_flags(m);
        return TK_OK;
    }
    if (kind == K_NEG) {
        alu = cpu_alu_sub(0, m->y);
        apply_flags(m, alu);
        set_sig(m, "Zeroout Sub Zin PSWup");
        set_func(m, "0-Y->Z 更新PSW");
        show_flags(m);
        return TK_OK;
    }
    if (kind == K_SHIFT) {
        return tick_shift(m);
    }
    if (kind == K_MUL) {
        alu = cpu_alu_mul(m->y, m->r[m->din.rt]);
        apply_flags(m, alu);
        set_sig(m, "RTout Mul Zin PSWup");
        set_func(m, "Y*r%d 低32位->Z", m->din.rt);
        show_flags(m);
        return TK_OK;
    }
    return tick_div(m, kind == K_MOD);
}

static int tick_branch(MmMachine *m) {
    if (!cpu_cond(m->din.cond, m->psw)) {
        set_sig(m, "无");
        set_func(m, "条件不成立");
        set_det(m, "—");
        return TK_OK;
    }
    if ((m->z & 3u) != 0) {
        set_func(m, "条件成立但目标未对齐");
        set_det(m, "目标=%08X", m->z);
        return mark_fault(m, MM_EX_ALIGN, m->z);
    }
    if ((m->psw & MM_PSW_U) != 0 && cpu_is_host(m->z)) {
        set_func(m, "用户态不能进入宿主入口");
        set_det(m, "目标=%08X", m->z);
        return mark_fault(m, MM_EX_PROT, m->z);
    }
    m->pc = m->z;
    set_sig(m, "Zout PCin");
    set_func(m, "条件成立 Z->PC");
    set_det(m, "PC=%08X", m->pc);
    return TK_OK;
}

static int tick_trap(MmMachine *m) {
    set_sig(m, "无");
    set_det(m, "PC=%08X", m->pc);
    if (m->din.op == MM_OP_TRAPV && (m->psw & MM_PSW_OF) == 0) {
        set_func(m, "OF=0 不自陷");
        return TK_OK;
    }
    if (m->din.op == MM_OP_TRAPV) {
        m->cause = MM_EX_OVF;
        set_func(m, "溢出陷阱");
    } else if (m->din.op == MM_OP_BRK) {
        m->cause = MM_EX_BREAK;
        set_func(m, "断点陷阱");
    } else {
        m->cause = MM_EX_SYSCALL;
        set_func(m, "系统调用陷阱");
    }
    return TK_TRAP;
}

static int tick_ctl(MmMachine *m, int kind) {
    const CpuInsn *in = &m->din;
    if (kind == K_BRANCH) {
        return tick_branch(m);
    }
    if (kind == K_EMPTY) {
        set_sig(m, "无");
        set_func(m, "空操作");
        set_det(m, "—");
        return TK_OK;
    }
    if (kind == K_HALT) {
        set_sig(m, "无");
        set_func(m, "停机");
        set_det(m, "—");
        return TK_HALT;
    }
    if (kind == K_EI || kind == K_DI) {
        if (kind == K_EI) {
            m->psw |= MM_PSW_IF;
        } else {
            m->psw &= ~MM_PSW_IF;
        }
        set_sig(m, "无");
        set_func(m, kind == K_EI ? "IF<-1" : "IF<-0");
        set_det(m, "PSW=%08X", m->psw & 0xffu);
        return TK_OK;
    }
    if (kind == K_IRET_PC) {
        uint32_t target = m->sr[MM_SR_EPC];
        if ((target & 3u) != 0) {
            set_func(m, "EPC->PC 未对齐");
            set_det(m, "EPC=%08X", target);
            return mark_fault(m, MM_EX_ALIGN, target);
        }
        if ((m->sr[MM_SR_EPSW] & MM_PSW_U) != 0 && cpu_is_host(target)) {
            set_func(m, "用户态不能回到宿主入口");
            set_det(m, "EPC=%08X", target);
            return mark_fault(m, MM_EX_PROT, target);
        }
        m->pc = target;
        set_sig(m, "SRout PCin");
        set_func(m, "EPC->PC");
        set_det(m, "PC=%08X", m->pc);
        return TK_OK;
    }
    if (kind == K_IRET_PSW) {
        m->psw = m->sr[MM_SR_EPSW] & 0xffu;
        m->cause = -1;
        set_sig(m, "SRout PSWin");
        set_func(m, "EPSW->PSW");
        set_det(m, "PSW=%08X", m->psw);
        return TK_OK;
    }
    if (kind == K_MFSR) {
        m->r[in->rd] = mm_sr(m, in->imm);
        set_sig(m, "SRout RDin");
        set_func(m, "sr%d->r%d", in->imm, in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    }
    if (kind == K_MTSR) {
        mm_set_sr(m, in->imm, m->r[in->rs]);
        set_sig(m, "RSout SRin");
        set_func(m, "r%d->sr%d", in->rs, in->imm);
        set_det(m, "sr=%08X", mm_sr(m, in->imm));
        return TK_OK;
    }
    if (kind == K_IN) {
        m->r[in->rd] = m->port[in->imm];
        set_sig(m, "IOout RDin");
        set_func(m, "端口%u->r%d", in->imm, in->rd);
        set_det(m, "r%d=%08X", in->rd, m->r[in->rd]);
        return TK_OK;
    }
    if (kind == K_OUT) {
        m->port[in->imm] = m->r[in->rs];
        if (in->imm == 0x70) {
            cpu_eoi(m, (int)m->r[in->rs]);
        }
        if (m->after_out != NULL) {
            m->after_out(m, in->imm);
        }
        set_sig(m, "RSout IOin");
        set_func(m, "r%d->端口%u", in->rs, in->imm);
        set_det(m, "端口=%08X", m->port[in->imm]);
        return TK_OK;
    }
    if (kind == K_TLB) {
        mmu_flush(m);
        set_sig(m, "无");
        set_func(m, "清空TLB");
        /* 没装 MMU 时保持阶段 00 的原文，已经发布的 trace 不变。 */
        set_det(m, mmu_on(m) ? "TLB已清空" : "阶段00无TLB");
        return TK_OK;
    }
    return tick_trap(m);
}

int cpu_tick(MmMachine *m, int kind) {
    snprintf(m->t_stage, sizeof m->t_stage, "%s", "执行");
    snprintf(m->t_sig, sizeof m->t_sig, "%s", "无");
    snprintf(m->t_func, sizeof m->t_func, "%s", "—");
    snprintf(m->t_detail, sizeof m->t_detail, "%s", "—");
    if (kind <= K_DECODE) {
        return cpu_tick_fetch(m, kind);
    }
    if (kind <= K_LUI) {
        return cpu_tick_move(m, kind);
    }
    if (kind <= K_WRITE_B) {
        return cpu_tick_mem(m, kind);
    }
    if (kind <= K_MOD) {
        return tick_alu(m, kind);
    }
    return tick_ctl(m, kind);
}
