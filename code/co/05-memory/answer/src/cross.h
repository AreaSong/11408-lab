#ifndef CROSS_H
#define CROSS_H

#include <stdint.h>
#include <stdio.h>

int bank_pairs(int modules, const uint64_t *addrs, int n, int *left, int *right, int cap, int *pairs);
int span_cycles(int bus_bytes, uint64_t addr, uint64_t nbytes);
int seq_pipeline_ok(int modules, double cycle_s, double bus_s);
double seq_seconds(int low, int modules, double cycle_s, double bus_s, int words);
double channel_bytes_per_s(int channels, double hz, int width_bits);

int show_bank(FILE *out, int modules, const uint64_t *addrs, int n);
int show_span(FILE *out, int modules, int bus_bits, uint64_t nbytes, uint64_t addr);
int show_seq(FILE *out, int modules, double cycle_s, double bus_s, int words, int word_bytes);
int show_bw(FILE *out, int channels, double hz, int width_bits);

#endif
