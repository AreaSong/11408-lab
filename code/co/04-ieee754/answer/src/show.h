#ifndef SHOW_H
#define SHOW_H

#include <stdio.h>

int show_enc(FILE *out, const char *text);
int show_dec(FILE *out, const char *text);
int show_add(FILE *out, const char *a, const char *b, int is_sub);
int show_fields(FILE *out);
int show_fit(FILE *out, int ka, int kb);
int show_cast(FILE *out, const char *text);
int show_sum(FILE *out, unsigned n);

#endif
