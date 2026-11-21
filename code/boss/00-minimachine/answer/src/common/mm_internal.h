#ifndef MM_INTERNAL_H
#define MM_INTERNAL_H

#include "common/mm_machine.h"
#include "cpu/cpu_decode.h"
#include "cpu/cpu_intr.h"

struct KWorld;

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* 时空图画得下的动态指令数和时钟列。超出仍照常执行，只是格子不再记。 */
#define MM_PIPE_LOG 24
#define MM_PIPE_CYC 64

typedef struct {
    int valid;
    int seq;
    uint32_t pc;
    uint32_t npc;
    CpuInsn in;
    int dest;
    int ex0;
    int ex1;
    int memr;
    int is_load;
    int uses_mem;
    int is_branch;
    int writes_flags;
    int reads_flags;
    int halt;
    int psw_write;
    int trap;
    int taken;
    int fault;
    uint32_t badva;
    uint32_t result;
    uint32_t ea;
    uint32_t flag_bits;
    uint32_t new_psw;
    uint32_t target;
} PipeSlot;

struct MmMachine {
    uint32_t r[8];
    uint32_t pc;
    uint32_t psw;
    uint32_t ir;
    uint32_t mar;
    uint32_t mdr;
    uint32_t y;
    uint32_t z;
    uint32_t ipc;
    uint32_t ret_save;
    uint32_t sr[8];
    uint32_t *port;
    uint8_t *mem;
    uint32_t mem_size;
    int halted;
    int cause;
    int trap;
    uint32_t badva;
    int bus_bad;
    uint32_t cycles;
    uint32_t insns;
    uint32_t cat_n[5];
    uint32_t cat_c[5];
    uint32_t step_base;
    FILE *trace;
    int t_index;
    int op_mem;
    int fetch_mem;
    CpuInsn din;
    char t_stage[16];
    char t_func[96];
    char t_detail[160];
    char t_sig[80];
    MmMemHook hook_read;
    MmMemHook hook_write;
    CpuHandler handler[32];
    MmQuietFn after_intr;
    MmQuietFn k_step;
    struct KWorld *kworld;
    FILE *event;
    uint16_t irq_pending;
    uint16_t irq_service;
    int nmi_pending;
    int nmi_service;
    int asleep;
    int exited;
    int exit_code;
    int deliver_depth;
    int irq_depth;
    uint32_t irq_mask[16];
    uint8_t irq_steps[16];
    uint32_t timer_acc;
    int nest_len;
    int nest_seq[32];
    int last_len;
    int last_seq[32];
    char con_out[512];
    int con_len;
    char con_in[256];
    int con_in_len;
    int con_in_pos;
    /* 阶段 02。配置在 mm_reset 时恢复默认，命令层会再套上。 */
    int pipe_on;
    int pipe_forward;
    int pipe_branch;
    int pipe_unified;
    int pipe_diagram;
    uint32_t pipe_mhz;
    int pipe_cycles;
    int pipe_committed;
    int pipe_flushed;
    int pipe_struct;
    int pipe_data;
    int pipe_ctrl;
    int pipe_nlog;
    int pipe_clog;
    int pipe_overflow;
    uint32_t pipe_log_pc[MM_PIPE_LOG];
    int pipe_log_id[MM_PIPE_LOG];
    int pipe_log_if[MM_PIPE_LOG];
    int pipe_log_branch[MM_PIPE_LOG];
    int pipe_log_flush[MM_PIPE_LOG];
    int pipe_log_ctrl[MM_PIPE_LOG];
    char pipe_log_text[MM_PIPE_LOG][48];
    char pipe_cell[MM_PIPE_LOG][MM_PIPE_CYC];
    PipeSlot pipe_st[5];
    /* 阶段 03。cache 为 NULL 时访存直通主存，周期与阶段 02 相同。 */
    struct MemCache *cache;
    int mem_if;
    int pend_i;
    int pend_d;
    int mem_stall;
    /* 阶段 04。mmu 为 NULL 时，PG=1 仍按阶段 02：流水线数据访问直接缺页。 */
    struct Mmu *mmu;
    int mmu_stall;
};

void mm_err(char *err, size_t cap, const char *fmt, ...);
int mm_parse_i64(const char *text, int64_t *out, char *err, size_t cap);

#endif
