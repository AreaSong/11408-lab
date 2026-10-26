#ifndef LINE_H
#define LINE_H

#define TOK_N 16
#define TOK_W 160

int fail(const char *msg);
int need(int got, int want);
int split_toks(const char *text, char tok[][TOK_W], int *ntok);
int read_cmd(char *buf, int cap);
int take_i(const char *text, int lo, int hi, int *out);
int take_u64(const char *text, unsigned long long lo, unsigned long long hi,
             unsigned long long *out);
int token_name(const char *text, int lo, int hi);
int token_path(const char *text);

#endif
