#ifndef EXTRA_H
#define EXTRA_H

#include "read.h"

int cmd_phi(const char *name, int n);
int cmd_bound(char tok[][TOK_LEN], int ntok);
int cmd_card(void);
int cmd_claim(const char *name);

#endif
