#ifndef DRAM_H
#define DRAM_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint64_t rows;
    uint64_t cols;
    int row_bits;
    int col_bits;
    int addr_pins;
} DramLayout;

typedef struct {
    uint64_t slots;
    double dead_s;
    double dead_ratio;
    uint64_t rw_slots;
    double sys_cycle_s;
    double sweep_s;
    double async_gap_s;
    int dead_fits;
} RefreshPlan;

/* 单元数须为 2 的幂。先让地址引脚最少，引脚相同再让行更少：刷新按行，行少开销小。 */
int dram_layout(uint64_t cells, DramLayout *best, DramLayout *other, int *has_other);
int refresh_plan(uint64_t rows, double interval_s, double cycle_s, RefreshPlan *out);

int show_dram(FILE *out, uint64_t cells, int data_bits);
int show_refresh(FILE *out, uint64_t rows, double interval_s, double cycle_s);

#endif
