#include "cpu/cpu_pipe.h"

#include "common/mm_internal.h"

#include <stdio.h>
#include <string.h>

void cpu_pipe_reset(MmMachine *m) {
    memset(m->pipe_st, 0, sizeof m->pipe_st);
    memset(m->pipe_log_pc, 0, sizeof m->pipe_log_pc);
    memset(m->pipe_log_id, 0, sizeof m->pipe_log_id);
    memset(m->pipe_log_if, 0, sizeof m->pipe_log_if);
    memset(m->pipe_log_branch, 0, sizeof m->pipe_log_branch);
    memset(m->pipe_log_flush, 0, sizeof m->pipe_log_flush);
    memset(m->pipe_log_ctrl, 0, sizeof m->pipe_log_ctrl);
    memset(m->pipe_log_text, 0, sizeof m->pipe_log_text);
    memset(m->pipe_cell, 0, sizeof m->pipe_cell);
    m->pipe_on = 0;
    m->pipe_forward = 1;
    m->pipe_branch = 0;
    m->pipe_unified = 0;
    m->pipe_diagram = 0;
    m->pipe_mhz = 0;
    m->pipe_cycles = 0;
    m->pipe_committed = 0;
    m->pipe_flushed = 0;
    m->pipe_struct = 0;
    m->pipe_data = 0;
    m->pipe_ctrl = 0;
    m->pipe_nlog = 0;
    m->pipe_clog = 0;
    m->pipe_overflow = 0;
}

void cpu_pipe_opt_default(CpuPipeOpt *opt) {
    memset(opt, 0, sizeof *opt);
    opt->forward = 1;
}

void cpu_pipe_set(MmMachine *m, const CpuPipeOpt *opt, int enabled) {
    m->pipe_forward = opt->forward ? 1 : 0;
    m->pipe_branch = opt->branch_stall ? 1 : 0;
    m->pipe_unified = opt->unified ? 1 : 0;
    m->pipe_diagram = opt->diagram ? 1 : 0;
    m->pipe_mhz = opt->freq_mhz;
    m->pipe_on = enabled ? 1 : 0;
}

int cpu_pipe_enabled(const MmMachine *m) { return m->pipe_on; }
void cpu_pipe_stat(const MmMachine *m, CpuPipeStat *out) {
    memset(out, 0, sizeof *out);
    out->cycles = m->pipe_cycles;
    out->committed = m->pipe_committed;
    out->flushed = m->pipe_flushed;
    out->ideal = m->pipe_committed > 0 ? m->pipe_committed + 4 : 0;
    out->extra = out->cycles - out->ideal;
    out->data_stall = m->pipe_data;
    out->ctrl_stall = m->pipe_ctrl;
    out->struct_stall = m->pipe_struct;
}

int cpu_pipe_id_extra(const MmMachine *m, int seq) {
    if (seq < 0 || seq >= m->pipe_nlog) {
        return -1;
    }
    return m->pipe_log_id[seq] > 0 ? m->pipe_log_id[seq] - 1 : 0;
}

int cpu_pipe_ctrl_of(const MmMachine *m, int seq) {
    if (seq < 0 || seq >= m->pipe_nlog) {
        return -1;
    }
    return m->pipe_log_ctrl[seq];
}

static int gcd(int a, int b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static void fraction(FILE *fp, int num, int den) {
    int g = gcd(num, den);
    fprintf(fp, "%d/%d", num / g, den / g);
}

static const char *stage_name(int code) {
    static const char *name[] = {"", "IF", "ID", "EX", "MEM", "WB"};
    if (code < 1 || code > 5) {
        return "";
    }
    return name[code];
}

static void print_mode(const MmMachine *m, FILE *fp) {
    fprintf(fp, "【五段流水线】转发=%s 分支=%s 存储器=%s\n", m->pipe_forward ? "开" : "关",
            m->pipe_branch ? "阻塞到MEM" : "预测不转移", m->pipe_unified ? "统一" : "分开");
    fprintf(fp, "约定：IF 取指，ID 译码并读寄存器，EX 执行或算地址，MEM 访存，WB 写回。\n");
    fprintf(fp, "寄存器堆前半拍写、后半拍读。\n");
}

static void print_stalls(const MmMachine *m, FILE *fp) {
    int i;
    for (i = 0; i < m->pipe_nlog; i++) {
        int data = m->pipe_log_id[i] > 0 ? m->pipe_log_id[i] - 1 : 0;
        if (m->pipe_log_flush[i]) {
            continue;
        }
        if (data > 0) {
            fprintf(fp, "I%d 数据阻塞 %d 拍\n", i + 1, data);
        }
        if (m->pipe_log_ctrl[i] > 0) {
            fprintf(fp, "I%d 控制阻塞 %d 拍\n", i + 1, m->pipe_log_ctrl[i]);
        }
    }
    if (m->pipe_struct > 0) {
        fprintf(fp, "结构阻塞 %d 拍\n", m->pipe_struct);
    }
    if (m->pipe_flushed > 0) {
        fprintf(fp, "冲刷 %d 条\n", m->pipe_flushed);
    }
}

static void print_diagram(const MmMachine *m, FILE *fp) {
    int last = m->pipe_cycles;
    int i;
    int c;
    if (!m->pipe_diagram) {
        return;
    }
    if (m->pipe_overflow || last >= MM_PIPE_CYC || m->pipe_nlog >= MM_PIPE_LOG) {
        fprintf(fp, "指令较多，时空图省略\n");
        return;
    }
    fprintf(fp, "时空图\n时钟");
    for (c = 1; c <= last; c++) {
        fprintf(fp, "%5d", c);
    }
    fputc('\n', fp);
    for (i = 0; i < m->pipe_nlog; i++) {
        fprintf(fp, "I%-3d", i + 1);
        for (c = 1; c <= last; c++) {
            const char *name = stage_name((int)m->pipe_cell[i][c]);
            if (name[0] == '\0') {
                fprintf(fp, "     ");
            } else {
                fprintf(fp, "%5s", name);
            }
        }
        fputc('\n', fp);
    }
}

static void print_time(const MmMachine *m, FILE *fp, int cycles, int n) {
    uint32_t mhz;
    int cycle_ns;
    if (m->pipe_mhz == 0 || n <= 0 || cycles <= 0) {
        return;
    }
    mhz = m->pipe_mhz;
    fprintf(fp, "CPI = %d/%d = ", cycles, n);
    if (cycles % n == 0) {
        fprintf(fp, "%d\n", cycles / n);
    } else {
        fraction(fp, cycles, n);
        fprintf(fp, "\n");
    }
    if (1000u % mhz == 0u) {
        cycle_ns = (int)(1000u / mhz);
        fprintf(fp, "主频 %u MHz，时钟周期 %d ns\n", mhz, cycle_ns);
        fprintf(fp, "执行时间 = %d × %d ns = %d ns\n", cycles, cycle_ns, cycles * cycle_ns);
    } else {
        fprintf(fp, "主频 %u MHz\n", mhz);
        fprintf(fp, "执行时间 = %d / (%u × 10^6) s\n", cycles, mhz);
    }
    fprintf(fp, "MIPS = %u / (%d/%d) = ", mhz, cycles, n);
    if (((uint32_t)cycles * 1u) != 0 && ((uint32_t)mhz * (uint32_t)n) % (uint32_t)cycles == 0u) {
        fprintf(fp, "%u\n", (mhz * (uint32_t)n) / (uint32_t)cycles);
    } else {
        fraction(fp, (int)mhz * n, cycles);
        fprintf(fp, "\n");
    }
}

void cpu_pipe_report(const MmMachine *m, FILE *fp) {
    CpuPipeStat st;
    cpu_pipe_stat(m, &st);
    print_mode(m, fp);
    fprintf(fp, "提交 %d 条，理想 %d 个时钟周期，实际 %d 个时钟周期\n", st.committed, st.ideal, st.cycles);
    print_stalls(m, fp);
    print_diagram(m, fp);
    fprintf(fp, "结果：阻塞 %d 拍，共 %d 个时钟周期\n", st.extra, st.cycles);
    fprintf(fp, "数据 %d 拍，控制 %d 拍，结构 %d 拍\n", st.data_stall, st.ctrl_stall, st.struct_stall);
    if (m->mem_stall > 0) {
        fprintf(fp, "Cache 缺失阻塞 %d 拍，已算进上面的实际周期\n", m->mem_stall);
    }
    print_time(m, fp, st.cycles, st.committed);
    if (st.committed > 0 && st.cycles > 0) {
        fprintf(fp, "吞吐率 TP = %d 条 / %d 个时钟周期\n", st.committed, st.cycles);
        fprintf(fp, "效率 E = (%d×5)/(5×%d) = ", st.committed, st.cycles);
        fraction(fp, st.committed, st.cycles);
        fprintf(fp, "\n");
    }
}

void cpu_pipe_compare(FILE *fp, uint32_t tick_cycles, uint32_t tick_insns, const MmMachine *m, int same) {
    CpuPipeStat st;
    cpu_pipe_stat(m, &st);
    fprintf(fp, "对照 节拍周期=%u 流水线周期=%d\n", tick_cycles, st.cycles);
    fprintf(fp, "指令 节拍=%u 流水线=%d\n", tick_insns, st.committed);
    if (tick_cycles > 0 && st.cycles > 0) {
        fprintf(fp, "加速比 S = %u/%d = ", tick_cycles, st.cycles);
        fraction(fp, (int)tick_cycles, st.cycles);
        fprintf(fp, "\n");
    }
    if (st.committed > 0 && st.cycles > 0) {
        fprintf(fp, "吞吐率 TP = %d 条 / %d 个时钟周期\n", st.committed, st.cycles);
        fprintf(fp, "效率 E = ");
        fraction(fp, st.committed, st.cycles);
        fprintf(fp, "\n");
    }
    print_time(m, fp, st.cycles, st.committed);
    fprintf(fp, "%s\n", same ? "状态一致" : "状态不一致");
}
