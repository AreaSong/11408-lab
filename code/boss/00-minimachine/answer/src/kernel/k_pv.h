#ifndef K_PV_H
#define K_PV_H

#include <stdio.h>

enum { K_PV_PC = 0, K_PV_WRONG, K_PV_READ, K_PV_PHI, K_PV_ROOM, K_PV_N };

typedef struct {
    int deadlock;
    int produced;
    int consumed;
    int meals;
    int max_read;
    int cycle;
} KPvOut;

int k_pv_id(const char *name);
const char *k_pv_name(int kind);
int k_pv_run(int kind, KPvOut *out);
void k_pv_print(int kind, const KPvOut *out, FILE *fp);

#endif
