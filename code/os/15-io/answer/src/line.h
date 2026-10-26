#ifndef LINE_H
#define LINE_H

#define TOK_N 12
#define TOK_W 64

int fail(const char *msg);
int need(int got, int want);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);
int take_i(const char *text, int lo, int hi, int *out);
int token_name(const char *text, int lo, int hi);

#endif
