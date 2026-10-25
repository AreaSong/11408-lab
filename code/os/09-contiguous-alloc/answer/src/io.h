#ifndef IO_H
#define IO_H

#define TOK_N 6
#define TOK_W 32

int fail(const char *msg);
int need_args(int got, int want);
int read_u(const char *text, long long *out);
int read_size(const char *text, long long *out);
int good_name(const char *text);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);

#endif
