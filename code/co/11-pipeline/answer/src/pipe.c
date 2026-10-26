#include "pipe.h"

#include <string.h>

enum { ST_IF, ST_ID, ST_EX, ST_MEM, ST_WB };

static const char CELL[] = "FDXMW";

int pipe_opt_named(const char *name, PipeOpt *opt) {
    memset(opt, 0, sizeof *opt);
    if (strcmp(name, "late") == 0) {
        return 0;
    }
    if (strcmp(name, "half") == 0) {
        opt->same_cycle = 1;
        return 0;
    }
    if (strcmp(name, "forward") == 0) {
        opt->forward = 1;
        opt->same_cycle = 1;
        return 0;
    }
    if (strcmp(name, "branch") == 0) {
        opt->forward = 1;
        opt->same_cycle = 1;
        opt->branch_stall = 1;
        return 0;
    }
    if (strcmp(name, "half-branch") == 0) {
        opt->same_cycle = 1;
        opt->branch_stall = 1;
        return 0;
    }
    if (strcmp(name, "struct") == 0) {
        opt->forward = 1;
        opt->same_cycle = 1;
        opt->unified = 1;
        return 0;
    }
    if (strcmp(name, "predict") == 0) {
        opt->forward = 1;
        opt->same_cycle = 1;
        opt->predict = 1;
        return 0;
    }
    if (strcmp(name, "slot") == 0) {
        opt->forward = 1;
        opt->same_cycle = 1;
        opt->delay_slot = 1;
        return 0;
    }
    return -1;
}

static int stage_of(const int *slot, int idx) {
    int s;
    for (s = 0; s < 5; s++) {
        if (slot[s] == idx) {
            return s;
        }
    }
    return -1;
}

static int producer(const Insn *ins, int i, int reg) {
    int j;
    if (reg < 0) {
        return -1;
    }
    for (j = i - 1; j >= 0; j--) {
        if (ins[j].rd == reg) {
            return j;
        }
    }
    return -1;
}

static void clear_slot(int *slot) {
    int s;
    for (s = 0; s < 5; s++) {
        slot[s] = -1;
    }
}

/* 无转发时，结果必须进了寄存器堆才能在 ID 读。转发时，只有 load 的下一条在 EX 用数才来不及。 */
static int id_stall(const Insn *ins, const int *slot, const int *wb_at, const int *flushed,
                    int cycle, const PipeOpt *opt) {
    int i = slot[ST_ID];
    int k;
    int total;
    if (i < 0) {
        return 0;
    }
    total = ins[i].nex + ins[i].nmem;
    for (k = 0; k < total; k++) {
        int reg = k < ins[i].nex ? ins[i].ex[k] : ins[i].memsrc[k - ins[i].nex];
        int need_ex = k < ins[i].nex;
        int p = producer(ins, i, reg);
        int st;
        int written;
        if (p < 0 || flushed[p]) {
            continue;
        }
        st = stage_of(slot, p);
        written = wb_at[p];
        /* 被分支跳过的指令没有写下新值，读的是寄存器里的旧数。 */
        if (st < 0 && written == 0) {
            continue;
        }
        if (!opt->forward) {
            if (st == ST_WB && opt->same_cycle) {
                continue;
            }
            if (st < 0 && written > 0 && written < cycle) {
                continue;
            }
            return 1;
        }
        if (st < 0 && written > 0 && written <= cycle) {
            continue;
        }
        if (st == ST_MEM || st == ST_WB) {
            continue;
        }
        if (st == ST_EX && !(ins[p].is_load && need_ex)) {
            continue;
        }
        return 1;
    }
    return 0;
}

/* 下一条的 IF 若和某条 load/store 的 MEM 同拍，取指让路。分支则要等它离开 MEM。 */
static int fetch_blocked(const Insn *ins, int n, const int *slot, int pc, int next_mem,
                         const PipeOpt *opt) {
    int j;
    if (opt->unified && next_mem >= 0 && ins[next_mem].uses_mem) {
        return 1;
    }
    if (!opt->branch_stall || pc >= n) {
        return 0;
    }
    for (j = 0; j < n; j++) {
        int st;
        if (!ins[j].is_branch || pc != j + 1) {
            continue;
        }
        st = stage_of(slot, j);
        if (st == ST_IF || st == ST_ID || st == ST_EX) {
            return 1;
        }
    }
    return 0;
}

static void record(const int *slot, int cycle, int *id_n, int *wb_at, PipeStat *st) {
    int s;
    for (s = 0; s < 5; s++) {
        int i = slot[s];
        if (i < 0) {
            continue;
        }
        st->cell[i][cycle] = CELL[s];
        if (s == ST_IF) {
            st->if_extra[i]++;
            if (st->if_first[i] == 0) {
                st->if_first[i] = cycle;
            }
        } else if (s == ST_ID) {
            id_n[i]++;
        } else if (s == ST_WB) {
            wb_at[i] = cycle;
        }
    }
}

static void freeze(int *next, const int *slot) {
    clear_slot(next);
    next[ST_WB] = slot[ST_MEM];
    next[ST_MEM] = slot[ST_EX];
    next[ST_ID] = slot[ST_ID];
    next[ST_IF] = slot[ST_IF];
}

static void redirect_slot(const Insn *ins, const int *slot, int *pc, const PipeOpt *opt) {
    int id;
    if (!opt->delay_slot) {
        return;
    }
    id = slot[ST_ID];
    if (id >= 0 && ins[id].is_branch && ins[id].taken && ins[id].target >= 0) {
        *pc = ins[id].target;
    }
}

static void flow(int *next, const int *slot, int *pc, int n, const Insn *ins, const PipeOpt *opt) {
    int next_mem;
    clear_slot(next);
    next[ST_WB] = slot[ST_MEM];
    next[ST_MEM] = slot[ST_EX];
    next[ST_EX] = slot[ST_ID];
    next[ST_ID] = slot[ST_IF];
    next_mem = next[ST_MEM];
    redirect_slot(ins, slot, pc, opt);
    if (*pc < n && !fetch_blocked(ins, n, slot, *pc, next_mem, opt)) {
        next[ST_IF] = *pc;
        (*pc)++;
    }
}

static void flush_taken(int *next, const int *slot, int *pc, const Insn *ins, int *flushed) {
    int ex = slot[ST_EX];
    int target = ins[ex].target;
    clear_slot(next);
    next[ST_WB] = slot[ST_MEM];
    next[ST_MEM] = ex;
    if (slot[ST_ID] >= 0 && slot[ST_ID] != target) {
        flushed[slot[ST_ID]] = 1;
    }
    if (slot[ST_IF] >= 0 && slot[ST_IF] != target) {
        flushed[slot[ST_IF]] = 1;
    }
    *pc = target;
    if (*pc >= 0) {
        next[ST_IF] = *pc;
        (*pc)++;
    }
}

static int taken_in_ex(const int *slot, const Insn *ins, const PipeOpt *opt) {
    int ex = slot[ST_EX];
    return opt->predict && ex >= 0 && ins[ex].is_branch && ins[ex].taken && ins[ex].target >= 0;
}

static void finish(const Insn *ins, int n, const int *id_n, const int *wb_at, const int *flushed,
                   const PipeOpt *opt, PipeStat *st) {
    int i;
    int committed = 0;
    st->nflush = 0;
    st->cycles = 0;
    for (i = 0; i < n; i++) {
        st->flushed[i] = flushed[i];
        st->id_extra[i] = id_n[i] > 0 ? id_n[i] - 1 : 0;
        st->if_extra[i] = st->if_extra[i] > 0 ? st->if_extra[i] - 1 : 0;
        if (flushed[i]) {
            st->nflush++;
        }
        if (st->if_first[i] > 0 && !flushed[i]) {
            committed++;
            if (wb_at[i] > st->cycles) {
                st->cycles = wb_at[i];
            }
        }
        if (opt->branch_stall && i > 0 && ins[i - 1].is_branch && st->if_first[i] > 0 &&
            st->if_first[i - 1] > 0) {
            int gap = st->if_first[i] - st->if_first[i - 1] - 1;
            st->ctrl[i] = gap - st->id_extra[i - 1];
            if (st->ctrl[i] < 0) {
                st->ctrl[i] = 0;
            }
        }
    }
    st->committed = committed;
    st->ideal = committed > 0 ? 5 + committed - 1 : 0;
    st->extra = st->cycles - st->ideal;
}

static int bad_branch(const Insn *ins, int n, const PipeOpt *opt) {
    int i;
    if (!opt->predict && !opt->delay_slot) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (ins[i].is_branch && ins[i].taken &&
            (ins[i].target < 0 || ins[i].target >= n)) {
            return 1;
        }
    }
    return 0;
}

int pipe_run(const Insn *ins, int n, const PipeOpt *opt, PipeStat *st) {
    int slot[5];
    int next[5];
    int wb_at[INSN_N];
    int id_n[INSN_N];
    int flushed[INSN_N];
    int pc = 0;
    int cycle;
    int s;
    if (n <= 0 || n > INSN_N || bad_branch(ins, n, opt)) {
        return -1;
    }
    memset(st, 0, sizeof *st);
    memset(wb_at, 0, sizeof wb_at);
    memset(id_n, 0, sizeof id_n);
    memset(flushed, 0, sizeof flushed);
    clear_slot(slot);
    slot[ST_IF] = 0;
    pc = 1;
    for (cycle = 1; cycle < PIPE_C; cycle++) {
        int empty = 1;
        for (s = 0; s < 5; s++) {
            if (slot[s] >= 0) {
                empty = 0;
            }
        }
        if (empty && pc >= n) {
            break;
        }
        record(slot, cycle, id_n, wb_at, st);
        if (taken_in_ex(slot, ins, opt)) {
            flush_taken(next, slot, &pc, ins, flushed);
        } else if (id_stall(ins, slot, wb_at, flushed, cycle, opt)) {
            freeze(next, slot);
        } else {
            flow(next, slot, &pc, n, ins, opt);
        }
        memcpy(slot, next, sizeof slot);
    }
    finish(ins, n, id_n, wb_at, flushed, opt, st);
    for (s = 0; s < n; s++) {
        if (st->if_first[s] > 0 && !flushed[s] && wb_at[s] == 0) {
            return -1;
        }
    }
    return 0;
}
