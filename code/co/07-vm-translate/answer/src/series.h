#ifndef SERIES_H
#define SERIES_H

#include <stdint.h>
#include <stdio.h>

/* 把虚页号当成块大小为 1 的地址，用第 06 关的 LRU 组相联选出被替换的虚页号。 */
int series_victims(int vpn_bits, int n_lines, int n_ways, const uint64_t *vpns, int n, uint64_t *victims,
                   int *n_victims, int *hits);

int show_series(FILE *out, int va_bits, int page_bytes, int n_lines, int n_ways, const uint64_t *vpns, int n);

#endif
