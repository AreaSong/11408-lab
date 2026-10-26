#ifndef IO_H
#define IO_H

#define TOK_N 12
#define TOK_W 32

int fail(const char *msg);
int need_args(int got, int want);
int read_ull(const char *text, unsigned long long *out);
int read_args(char tok[][TOK_W], int from, int n, unsigned long long *out);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);
int pow2_bits(unsigned long long value, int *bits);
int mul_ull(unsigned long long a, unsigned long long b, unsigned long long *out);
int add_ull(unsigned long long a, unsigned long long b, unsigned long long *out);
void print_hex(unsigned long long value);

#endif
