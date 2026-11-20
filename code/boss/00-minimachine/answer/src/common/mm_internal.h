#ifndef MM_INTERNAL_H
#define MM_INTERNAL_H

#include "common/mm_machine.h"
#include "cpu/cpu_decode.h"
#include "cpu/cpu_intr.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

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
};

void mm_err(char *err, size_t cap, const char *fmt, ...);
int mm_parse_i64(const char *text, int64_t *out, char *err, size_t cap);

#endif
