#ifndef MM_INTERNAL_H
#define MM_INTERNAL_H

#include "common/mm_machine.h"
#include "cpu/cpu_decode.h"

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
};

void mm_err(char *err, size_t cap, const char *fmt, ...);
int mm_parse_i64(const char *text, int64_t *out, char *err, size_t cap);

#endif
