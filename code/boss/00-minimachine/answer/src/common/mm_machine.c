#include "common/mm_internal.h"
#include "asm/asm_image.h"
#include "cpu/cpu_intr.h"
#include "cpu/cpu_pipe.h"
#include "dev/dev_port.h"
#include "net/net_nic.h"
#include "kernel/k_file.h"
#include "kernel/k_proc.h"
#include "mem/mem_cache.h"
#include "mmu/mmu_walk.h"

#include <stdlib.h>
#include <string.h>

static void clear_arch(MmMachine *m) {
    k_fs_detach(m);
    k_proc_drop(m);
    mem_cache_drop(m);
    mmu_drop(m);
    memset(m->r, 0, sizeof m->r);
    memset(m->sr, 0, sizeof m->sr);
    m->pc = 0x00001000u;
    m->psw = 0;
    m->ir = 0;
    m->mar = 0;
    m->mdr = 0;
    m->y = 0;
    m->z = 0;
    m->ipc = 0;
    m->ret_save = 0;
    m->halted = 0;
    m->cause = -1;
    m->trap = 0;
    m->badva = 0;
    m->bus_bad = 0;
    m->cycles = 0;
    m->insns = 0;
    memset(m->cat_n, 0, sizeof m->cat_n);
    memset(m->cat_c, 0, sizeof m->cat_c);
    if (m->mem != NULL) {
        memset(m->mem, 0, m->mem_size);
    }
    if (m->port != NULL) {
        memset(m->port, 0, sizeof(uint32_t) * 65536u);
    }
    cpu_intr_reset(m);
    cpu_pipe_reset(m);
    dev_after_reset(m);
    net_after_reset(m);
}

MmMachine *mm_create(uint32_t mem_bytes) {
    MmMachine *m = calloc(1, sizeof *m);
    if (m == NULL) {
        return NULL;
    }
    if (mem_bytes == 0) {
        mem_bytes = MM_MEM_MAX;
    }
    m->mem = calloc(mem_bytes, 1);
    m->port = calloc(65536u, sizeof(uint32_t));
    if (m->mem == NULL || m->port == NULL) {
        mm_destroy(m);
        return NULL;
    }
    m->mem_size = mem_bytes;
    clear_arch(m);
    m->cause = -1;
    return m;
}

void mm_destroy(MmMachine *m) {
    if (m == NULL) {
        return;
    }
    dev_drop(m);
    net_drop(m);
    k_proc_drop(m);
    k_fs_drop(m);
    mem_cache_drop(m);
    mmu_drop(m);
    free(m->mem);
    free(m->port);
    free(m);
}

void mm_reset(MmMachine *m) {
    FILE *trace = m->trace;
    FILE *event = m->event;
    MmMemHook rd = m->hook_read;
    MmMemHook wr = m->hook_write;
    clear_arch(m);
    m->trace = trace;
    m->event = event;
    m->hook_read = rd;
    m->hook_write = wr;
}

const char *mm_cause_name(int cause) {
    switch (cause) {
    case MM_EX_DIV: return "除法错";
    case MM_EX_BREAK: return "断点";
    case MM_EX_NMI: return "不可屏蔽中断";
    case MM_EX_ILL: return "非法指令";
    case MM_EX_PRIV: return "特权指令违例";
    case MM_EX_PAGE: return "缺页";
    case MM_EX_PROT: return "越权访问";
    case MM_EX_ALIGN: return "地址未对齐";
    case MM_EX_SYSCALL: return "系统调用";
    case MM_EX_OVF: return "溢出陷阱";
    case MM_EX_MCHK: return "机器检查";
    default: return "未知";
    }
}

static int bad_size(int size) {
    return size != 1 && size != 4;
}

int mm_raw_read(MmMachine *m, uint32_t addr, int size, uint32_t *data) {
    uint32_t value = 0;
    int i;
    if (bad_size(size)) {
        m->badva = addr;
        return MM_EX_MCHK;
    }
    if (size == 4 && (addr & 3u) != 0) {
        m->badva = addr;
        return MM_EX_ALIGN;
    }
    if (m->dev != NULL && dev_mmio(m, addr, size, data, 0) == 0) {
        return 0;
    }
    if (addr > m->mem_size || size > (int)(m->mem_size - addr)) {
        m->badva = addr;
        return MM_EX_MCHK;
    }
    for (i = 0; i < size; i++) {
        value |= (uint32_t)m->mem[addr + (uint32_t)i] << (8 * i);
    }
    *data = value;
    return 0;
}

int mm_raw_write(MmMachine *m, uint32_t addr, int size, uint32_t data) {
    int i;
    if (bad_size(size)) {
        m->badva = addr;
        return MM_EX_MCHK;
    }
    if (size == 4 && (addr & 3u) != 0) {
        m->badva = addr;
        return MM_EX_ALIGN;
    }
    if (m->dev != NULL && dev_mmio(m, addr, size, &data, 1) == 0) {
        return 0;
    }
    if (addr > m->mem_size || size > (int)(m->mem_size - addr)) {
        m->badva = addr;
        return MM_EX_MCHK;
    }
    for (i = 0; i < size; i++) {
        m->mem[addr + (uint32_t)i] = (uint8_t)((data >> (8 * i)) & 0xffu);
    }
    return 0;
}

int mm_read(MmMachine *m, uint32_t addr, int size, uint32_t *data) {
    if (size == 4 && (addr & 3u) != 0) {
        m->badva = addr;
        return MM_EX_ALIGN;
    }
    if (m->hook_read != NULL) {
        return m->hook_read(m, addr, size, data);
    }
    if (m->cache != NULL) {
        return mem_cpu_read(m, addr, size, data);
    }
    return mm_raw_read(m, addr, size, data);
}

int mm_write(MmMachine *m, uint32_t addr, int size, uint32_t data) {
    if (size == 4 && (addr & 3u) != 0) {
        m->badva = addr;
        return MM_EX_ALIGN;
    }
    if (m->hook_write != NULL) {
        return m->hook_write(m, addr, size, &data);
    }
    if (m->cache != NULL) {
        return mem_cpu_write(m, addr, size, data);
    }
    return mm_raw_write(m, addr, size, data);
}

int mm_load(MmMachine *m, const AsmImage *img) {
    uint32_t addr;
    if (img == NULL) {
        return -1;
    }
    for (addr = 0; addr < img->space; addr++) {
        if (!img->mark[addr]) {
            continue;
        }
        if (addr >= m->mem_size) {
            return -1;
        }
        m->mem[addr] = img->mem[addr];
    }
    m->pc = img->entry;
    m->halted = 0;
    m->cause = -1;
    return 0;
}

uint32_t mm_reg(const MmMachine *m, int index) {
    return (index >= 0 && index < 8) ? m->r[index] : 0;
}

void mm_set_reg(MmMachine *m, int index, uint32_t value) {
    if (index >= 0 && index < 8) {
        m->r[index] = value;
    }
}

uint32_t mm_pc(const MmMachine *m) { return m->pc; }
void mm_set_pc(MmMachine *m, uint32_t pc) { m->pc = pc; }
uint32_t mm_psw(const MmMachine *m) { return m->psw & 0xffu; }

void mm_set_psw(MmMachine *m, uint32_t psw) { m->psw = psw & 0xffu; }

uint32_t mm_sr(const MmMachine *m, int index) {
    if (index == MM_SR_PSW) {
        return m->psw & 0xffu;
    }
    if (index < 0 || index > MM_SR_VBR) {
        return 0;
    }
    return m->sr[index];
}

void mm_set_sr(MmMachine *m, int index, uint32_t value) {
    if (index == MM_SR_PSW) {
        m->psw = value & 0xffu;
        return;
    }
    if (index >= 0 && index <= MM_SR_VBR) {
        m->sr[index] = value;
    }
}

uint32_t mm_badva(const MmMachine *m) { return m->badva; }
int mm_halted(const MmMachine *m) { return m->halted; }
int mm_cause(const MmMachine *m) { return m->cause; }
uint32_t mm_cycles(const MmMachine *m) { return m->cycles; }
uint32_t mm_insn_count(const MmMachine *m) { return m->insns; }
int mm_bus_conflicts(const MmMachine *m) { return m->bus_bad; }
uint32_t mm_mem_size(const MmMachine *m) { return m->mem_size; }

void mm_set_trace(MmMachine *m, FILE *fp) { m->trace = fp; }

int mm_set_hook(MmMachine *m, int id, MmMemHook fn) {
    if (id == MM_HOOK_READ) {
        m->hook_read = fn;
        return 0;
    }
    if (id == MM_HOOK_WRITE) {
        m->hook_write = fn;
        return 0;
    }
    return -1;
}

void mm_print_regs(const MmMachine *m, FILE *fp) {
    int i;
    uint32_t psw = m->psw & 0xffu;
    for (i = 0; i < 8; i++) {
        fprintf(fp, "%sr%d=%08X", i ? " " : "", i, m->r[i]);
    }
    fprintf(fp, "\nPC=%08X PSW=%08X ZF=%d SF=%d CF=%d OF=%d IF=%d U=%d PG=%d TF=%d\n",
            m->pc, psw, (psw & MM_PSW_ZF) != 0, (psw & MM_PSW_SF) != 0,
            (psw & MM_PSW_CF) != 0, (psw & MM_PSW_OF) != 0, (psw & MM_PSW_IF) != 0,
            (psw & MM_PSW_U) != 0, (psw & MM_PSW_PG) != 0, (psw & MM_PSW_TF) != 0);
}

void mm_print_stats(const MmMachine *m, FILE *fp) {
    static const char *name[] = {"运算", "访存", "转移", "系统", "其他"};
    uint32_t n = m->insns;
    uint32_t c = m->cycles;
    int i;
    if (n == 0) {
        fprintf(fp, "统计 指令=0 时钟周期=%u CPI=0\n", c);
    } else {
        unsigned whole = c / n;
        unsigned frac = (unsigned)(((uint64_t)(c % n) * 1000u) / n);
        fprintf(fp, "统计 指令=%u 时钟周期=%u CPI=%u/%u=%u.%03u\n", n, c, c, n, whole, frac);
    }
    fprintf(fp, "分类");
    for (i = 0; i < 5; i++) {
        fprintf(fp, " %s %u条 %u拍", name[i], m->cat_n[i], m->cat_c[i]);
    }
    fprintf(fp, "\n");
}
