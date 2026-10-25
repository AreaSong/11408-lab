#ifndef JCC_H
#define JCC_H

#include <stdio.h>

int jcc_find(const char *name);

int jcc_always(int id);

int jcc_taken(int id, int zf, int sf, int cf, int of);

int show_jump(FILE *out, const char *name);

#endif
