#ifndef IO_H
#define IO_H

#include "sim.h"

#define TOK_N 64
#define TOK_W 32

int fail(const char *msg);
int read_range(const char *text, int lo, int hi, int *out);
int parse_ref(const char *text, Access *out);
int parse_cell(const char *text, int *page, int *aref, int *dirty);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);

#endif
