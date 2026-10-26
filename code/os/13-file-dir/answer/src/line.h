#ifndef LINE_H
#define LINE_H

#define TOK_N 8
#define TOK_W 64

int fail(const char *msg);
int need(int got, int want);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);
int read_i(const char *text, int *out);
int read_mode(const char *text, int *out);

#endif
