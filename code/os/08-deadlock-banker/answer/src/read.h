#ifndef READ_H
#define READ_H

#include "num.h"

#define TOK_MAX 16
#define TOK_LEN 32

int read_line(char *buf, int cap, const char *tip);
int parse_ints(const char *text, int *out, int n);
int split_tok(const char *text, char tok[][TOK_LEN], int max, int *ntok);
int read_rows(int n, int m, int base, int rows[][BANK_M], const char *what);

#endif
