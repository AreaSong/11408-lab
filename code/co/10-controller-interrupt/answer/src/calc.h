#ifndef CALC_H
#define CALC_H

#include <stdio.h>

int group_sum(const int *groups, int n, int *naive);
int cmd_field(FILE *out, const char *args);
int cmd_next(FILE *out, const char *args);
int cmd_rom(FILE *out, const char *args);
int cmd_class(FILE *out, const char *args);
int print_groups(FILE *out, const int *groups, int n, int *sum, int *naive);
int print_next_body(FILE *out, int instructions, int common, int per, int *bits);
int print_rom_body(FILE *out, int word, int test_bits, const int *groups, int n,
                   int *next_bits, int *capacity);

#endif
