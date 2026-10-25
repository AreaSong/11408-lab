#ifndef SHOW_CALC_H
#define SHOW_CALC_H

#include "pipe.h"

#include <stddef.h>

/* kind：0 停等，1 GBN，2 SR，3 题面没点名协议的滑动窗口。 */

void fmt_milli(char *buf, size_t n, int milli, const char *unit);

void fmt_short_rate(char *buf, size_t n, long long bps);

void print_timing(const PipeDelay *d, int ack_same);

void print_window_need(const PipeDelay *d, int percent, int window);

void print_bits(int kind, int window);

#endif
