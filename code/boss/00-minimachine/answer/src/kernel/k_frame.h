#ifndef K_FRAME_H
#define K_FRAME_H

#include "common/mm_internal.h"

int k_push(MmMachine *m, uint32_t value);
int k_pop(MmMachine *m, uint32_t *out);

#endif
