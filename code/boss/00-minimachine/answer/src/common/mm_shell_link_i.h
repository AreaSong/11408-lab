#ifndef MM_SHELL_LINK_I_H
#define MM_SHELL_LINK_I_H

#include <stdio.h>

int mm_link_rexmit(const char *arg, FILE *out);
int mm_link_span(const char *arg, FILE *out);
int mm_link_cut(const char *arg, FILE *out);
int mm_link_phy(const char *arg, FILE *out);
int mm_link_bus(const char *arg, FILE *out);
int mm_link_simple(const char *s, FILE *out);

#endif
