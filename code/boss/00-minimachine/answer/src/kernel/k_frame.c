#include "kernel/k_frame.h"

int k_push(MmMachine *m, uint32_t value) {
    uint32_t sp = mm_sr(m, MM_SR_KSP) - 4u;
    int rc = mm_raw_write(m, sp, 4, value);
    if (rc != 0) {
        return rc;
    }
    mm_set_sr(m, MM_SR_KSP, sp);
    return 0;
}

int k_pop(MmMachine *m, uint32_t *out) {
    uint32_t sp = mm_sr(m, MM_SR_KSP);
    int rc = mm_raw_read(m, sp, 4, out);
    if (rc != 0) {
        return rc;
    }
    mm_set_sr(m, MM_SR_KSP, sp + 4u);
    return 0;
}
