#ifndef EXAM_H
#define EXAM_H

#include <stdint.h>
#include <stdio.h>

int bits_needed(int kinds);
int addr_bits(uint64_t n);
uint16_t encode15(int op, int rd, int m1, int rs1, int m2, int rs2);
int micro_len_sub(void);
int micro_len_inc(void);
int fetch_clocks(int mem_clocks);
int gpr_count(int index_bits);
int min_machine_cycles(void);
int reg_visible(const char *name);
char pick_letter(const char *id);

int show_2009(FILE *out);
int show_2015_path(FILE *out);
int show_2015_code(FILE *out);
int show_2022(FILE *out);
int show_cycle(FILE *out);
int show_pick(FILE *out, const char *id);

#endif
