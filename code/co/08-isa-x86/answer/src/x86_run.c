#include "x86.h"

#include "jcc.h"
#include "x86_int.h"

#include <stdio.h>
#include <string.h>

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };

typedef struct {
    uint32_t addr;
    uint32_t val;
} Slot;

typedef struct {
    uint32_t r[8];
    int zf, sf, cf, of;
    Slot slot[64];
    int nslot;
} Cpu;

static const char *rname(int id) {
    static const char *names[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    if (id < 0 || id > 7) {
        return "?";
    }
    return names[id];
}

static void alu32(uint32_t a, uint32_t b, int sub, uint32_t *sum, int *zf, int *sf, int *cf, int *of, int *cout) {
    uint32_t rhs = sub ? ~b : b;
    uint32_t cin = sub ? 1u : 0u;
    uint64_t total = (uint64_t)a + rhs + cin;
    uint32_t low = (a & 0x7fffffffu) + (rhs & 0x7fffffffu) + cin;
    int c_out = (int)(total >> 32);
    *sum = (uint32_t)total;
    *cout = c_out;
    *of = c_out ^ (int)(low >> 31);
    *cf = c_out ^ sub;
    *zf = *sum == 0;
    *sf = (int)(*sum >> 31);
}

static int mem_get(Cpu *cpu, uint32_t addr, uint32_t *val) {
    int i;
    if (addr % 4u != 0) {
        return -1;
    }
    for (i = 0; i < cpu->nslot; i++) {
        if (cpu->slot[i].addr == addr) {
            *val = cpu->slot[i].val;
            return 0;
        }
    }
    return -1;
}

static int mem_put(Cpu *cpu, uint32_t addr, uint32_t val) {
    int i;
    if (addr % 4u != 0) {
        return -1;
    }
    for (i = 0; i < cpu->nslot; i++) {
        if (cpu->slot[i].addr == addr) {
            cpu->slot[i].val = val;
            return 0;
        }
    }
    if (cpu->nslot >= 64) {
        return -1;
    }
    cpu->slot[cpu->nslot].addr = addr;
    cpu->slot[cpu->nslot].val = val;
    cpu->nslot++;
    return 0;
}

static uint32_t mem_ea(const Cpu *cpu, const X86Insn *ins) {
    return cpu->r[ins->mem_reg] + (uint32_t)ins->disp;
}

static int push_word(Cpu *cpu, uint32_t val) {
    if (cpu->r[R_ESP] < 4u) {
        return -1;
    }
    cpu->r[R_ESP] -= 4u;
    return mem_put(cpu, cpu->r[R_ESP], val);
}

static int pop_word(Cpu *cpu, uint32_t *val) {
    if (mem_get(cpu, cpu->r[R_ESP], val) != 0) {
        return -1;
    }
    cpu->r[R_ESP] += 4u;
    return 0;
}

static void save_flags(Cpu *cpu, int zf, int sf, int cf, int of) {
    cpu->zf = zf;
    cpu->sf = sf;
    cpu->cf = cf;
    cpu->of = of;
}

static void print_cmp(FILE *out, uint32_t sum, int zf, int sf, int cf, int of, int cout) {
    const char *us = zf ? "无符号相等" : (cf ? "无符号小于" : "无符号大于");
    const char *ss = zf ? "有符号相等" : (sf != of ? "有符号小于" : "有符号大于");
    fprintf(out, "    不写回寄存器。差=0x%X，Cout=%d，CF = Cout ⊕ 1 = %d，ZF=%d，SF=%d，OF=%d\n", sum, cout, cf, zf,
            sf, of);
    fprintf(out, "    %s，jbe %s；%s，jle %s\n", us, (cf || zf) ? "会转移" : "不转移", ss,
            (zf || sf != of) ? "会转移" : "不转移");
}

static int load_left(Cpu *cpu, const X86Insn *ins, uint32_t *left) {
    if (ins->mem_src) {
        return mem_get(cpu, mem_ea(cpu, ins), left);
    }
    if (ins->rd < 0) {
        return -1;
    }
    *left = cpu->r[ins->rd];
    return 0;
}

static int exec_alu(Cpu *cpu, const X86Insn *ins, FILE *out) {
    uint32_t left = 0;
    uint32_t right;
    uint32_t sum = 0;
    int zf, sf, cf, of, cout;
    if (load_left(cpu, ins, &left) != 0 || (!ins->imm_ok && ins->rs < 0)) {
        return -1;
    }
    right = ins->imm_ok ? ins->imm : cpu->r[ins->rs];
    alu32(left, right, ins->op != X_ADD, &sum, &zf, &sf, &cf, &of, &cout);
    save_flags(cpu, zf, sf, cf, of);
    if (ins->op != X_CMP) {
        cpu->r[ins->rd] = sum;
    }
    if (out == NULL) {
        return 0;
    }
    if (ins->op == X_CMP) {
        print_cmp(out, sum, zf, sf, cf, of, cout);
        return 0;
    }
    fprintf(out, "    %s=0x%X，ZF=%d，SF=%d，CF=%d，OF=%d\n", rname(ins->rd), sum, zf, sf, cf, of);
    return 0;
}

static int exec_mov(Cpu *cpu, const X86Insn *ins, FILE *out) {
    uint32_t val = 0;
    uint32_t addr;
    if (ins->mem_src) {
        addr = mem_ea(cpu, ins);
        if (mem_get(cpu, addr, &val) != 0) {
            return -1;
        }
        cpu->r[ins->rd] = val;
        if (out != NULL) {
            fprintf(out, "    %s=Mem[0x%X]=0x%X\n", rname(ins->rd), addr, val);
        }
        return 0;
    }
    val = ins->imm_ok ? ins->imm : cpu->r[ins->rs];
    if (ins->mem_dst) {
        addr = mem_ea(cpu, ins);
        if (mem_put(cpu, addr, val) != 0) {
            return -1;
        }
        if (out != NULL) {
            fprintf(out, "    Mem[0x%X]=0x%X\n", addr, val);
        }
        return 0;
    }
    cpu->r[ins->rd] = val;
    if (out != NULL) {
        fprintf(out, "    %s=0x%X\n", rname(ins->rd), val);
    }
    return 0;
}

static int exec_stack(Cpu *cpu, const X86Insn *ins, FILE *out, uint32_t *popped) {
    uint32_t val = 0;
    if (ins->op == X_PUSH) {
        val = ins->imm_ok ? ins->imm : cpu->r[ins->rs];
        if ((!ins->imm_ok && ins->rs < 0) || push_word(cpu, val) != 0) {
            return -1;
        }
        if (out != NULL) {
            fprintf(out, "    esp=0x%X，Mem[0x%X]=0x%X\n", cpu->r[R_ESP], cpu->r[R_ESP], val);
        }
        return 0;
    }
    if (pop_word(cpu, &val) != 0) {
        return -1;
    }
    if (ins->op == X_POP) {
        cpu->r[ins->rd] = val;
    }
    if (popped != NULL) {
        *popped = val;
    }
    if (out != NULL && ins->op == X_POP) {
        fprintf(out, "    %s=0x%X，esp=0x%X\n", rname(ins->rd), val, cpu->r[R_ESP]);
    }
    return 0;
}

static int find_addr(const X86Insn *prog, int n, uint32_t addr) {
    int i;
    for (i = 0; i < n; i++) {
        if (prog[i].addr == addr) {
            return i;
        }
    }
    return -1;
}

static void scan_shape(const X86Insn *prog, int n, X86End *end) {
    int i;
    end->branch = 0;
    end->loop = 0;
    end->call = 0;
    for (i = 0; i < n; i++) {
        if (prog[i].op == X_CALL) {
            end->call = 1;
        }
        if (prog[i].op != X_JUMP) {
            continue;
        }
        if (prog[i].target < i) {
            end->loop = 1;
        } else if (!jcc_always(prog[i].jcc)) {
            end->branch = 1;
        }
    }
}

static void print_shape(FILE *out, const X86End *end) {
    int any = 0;
    fprintf(out, "结构：");
    if (end->branch) {
        fprintf(out, "条件分支");
        any = 1;
    }
    if (end->loop) {
        fprintf(out, "%s循环", any ? "、" : "");
        any = 1;
    }
    if (end->call) {
        fprintf(out, "%s过程调用", any ? "、" : "");
        any = 1;
    }
    if (!any) {
        fprintf(out, "顺序");
    }
    fprintf(out, "\n");
}

static int exec_shift(Cpu *cpu, const X86Insn *ins, FILE *out) {
    uint32_t val;
    uint32_t n;
    if (ins->rd < 0) {
        return -1;
    }
    n = ins->imm & 31u;
    val = cpu->r[ins->rd];
    if (n > 0) {
        cpu->cf = (int)((val >> (32u - n)) & 1u);
        val <<= n;
        cpu->zf = val == 0;
        cpu->sf = (int)(val >> 31);
        cpu->of = n == 1u ? cpu->cf ^ cpu->sf : cpu->of;
        cpu->r[ins->rd] = val;
    }
    if (out != NULL) {
        fprintf(out, "    %s=0x%X，ZF=%d，SF=%d，CF=%d，OF=%d\n", rname(ins->rd), cpu->r[ins->rd], cpu->zf, cpu->sf,
                cpu->cf, cpu->of);
    }
    return 0;
}

static int exec_mul(Cpu *cpu, const X86Insn *ins, FILE *out) {
    int64_t prod;
    uint32_t low;
    int32_t hi;
    int32_t sign;
    if (ins->rd < 0 || ins->rs < 0) {
        return -1;
    }
    prod = (int64_t)(int32_t)cpu->r[ins->rd] * (int64_t)(int32_t)cpu->r[ins->rs];
    low = (uint32_t)prod;
    hi = (int32_t)(prod >> 32);
    sign = (low & 0x80000000u) ? -1 : 0;
    cpu->r[ins->rd] = low;
    cpu->cf = cpu->of = hi != sign;
    cpu->zf = low == 0;
    cpu->sf = (int)(low >> 31);
    if (out != NULL) {
        fprintf(out, "    %s=0x%X，ZF=%d，SF=%d，CF=%d，OF=%d\n", rname(ins->rd), low, cpu->zf, cpu->sf, cpu->cf,
                cpu->of);
    }
    return 0;
}

static int exec_jump(Cpu *cpu, const X86Insn *prog, int n, int ip, FILE *out) {
    const X86Insn *ins = &prog[ip];
    int taken;
    uint32_t ret = 0;
    if (ins->op == X_RET) {
        if (pop_word(cpu, &ret) != 0) {
            return -1;
        }
        if (out != NULL) {
            fprintf(out, "    返回 0x%08X，esp=0x%X\n", ret, cpu->r[R_ESP]);
        }
        return find_addr(prog, n, ret);
    }
    if (ins->op == X_CALL) {
        if (push_word(cpu, prog[ip + 1].addr) != 0) {
            return -1;
        }
        if (out != NULL) {
            fprintf(out, "    压入返回地址 0x%08X，esp=0x%X，进入 0x%08X\n", prog[ip + 1].addr, cpu->r[R_ESP],
                    prog[ins->target].addr);
        }
        return ins->target;
    }
    taken = jcc_taken(ins->jcc, cpu->zf, cpu->sf, cpu->cf, cpu->of);
    if (out != NULL) {
        fprintf(out, "    %s 0x%08X\n", taken ? "转到" : "不转移，下一条",
                taken ? prog[ins->target].addr : prog[ip].addr + 4u);
    }
    return taken ? ins->target : ip + 1;
}

static int exec_one(Cpu *cpu, X86Insn *prog, int n, int ip, FILE *out) {
    X86Insn *ins = &prog[ip];
    if (out != NULL) {
        fprintf(out, "0x%08X  %s\n", ins->addr, ins->text);
    }
    if (ins->op == X_MOV) {
        return exec_mov(cpu, ins, out) == 0 ? ip + 1 : -1;
    }
    if (ins->op == X_ADD || ins->op == X_SUB || ins->op == X_CMP) {
        return exec_alu(cpu, ins, out) == 0 ? ip + 1 : -1;
    }
    if (ins->op == X_SHL) {
        return exec_shift(cpu, ins, out) == 0 ? ip + 1 : -1;
    }
    if (ins->op == X_IMUL) {
        return exec_mul(cpu, ins, out) == 0 ? ip + 1 : -1;
    }
    if (ins->op == X_PUSH || ins->op == X_POP) {
        return exec_stack(cpu, ins, out, NULL) == 0 ? ip + 1 : -1;
    }
    if (ins->op == X_CALL && ip + 1 >= n) {
        return -1;
    }
    return exec_jump(cpu, prog, n, ip, out);
}

int x86_run(FILE *out, const char *const *lines, int n, X86End *end) {
    X86Insn prog[X86_INS];
    Cpu cpu;
    int count = 0;
    int ip = 0;
    int steps = 0;
    if (end == NULL || x86_load(lines, n, prog, &count) != 0) {
        return -1;
    }
    memset(&cpu, 0, sizeof cpu);
    memset(end, 0, sizeof *end);
    scan_shape(prog, count, end);
    if (out != NULL) {
        fprintf(out, "【汇编执行】\n");
        fprintf(out, "玩具机按 32 位 Intel 语法执行，栈向低地址增长。每条指令占 4 字节，只为了给 call 一个返回地址；真题里的 x86 指令是变长的\n");
    }
    while (ip >= 0 && ip < count) {
        if (++steps > 10000) {
            fprintf(stderr, "步数超过 10000，像是死循环\n");
            return -1;
        }
        ip = exec_one(&cpu, prog, count, ip, out);
    }
    if (ip < 0) {
        fprintf(stderr, "执行失败\n");
        return -1;
    }
    end->eax = cpu.r[R_EAX];
    end->zf = cpu.zf;
    end->sf = cpu.sf;
    end->cf = cpu.cf;
    end->of = cpu.of;
    if (out != NULL) {
        fprintf(out, "结果：eax=0x%X（%d），ZF=%d，SF=%d，CF=%d，OF=%d\n", end->eax, (int32_t)end->eax, end->zf,
                end->sf, end->cf, end->of);
        print_shape(out, end);
    }
    return 0;
}
