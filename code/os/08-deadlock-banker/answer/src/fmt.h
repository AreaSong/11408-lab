#ifndef FMT_H
#define FMT_H

/* 向量用逗号隔开，名字之间用空格，这样每行仍能按空白切开。 */
int fmt_vec(char *dst, int cap, const int *v, int m);
int fmt_name(char *dst, int cap, int index, int base);
int fmt_names(char *dst, int cap, const int *idx, int n, int base);

#endif
