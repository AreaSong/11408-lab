#ifndef RUN_H
#define RUN_H

#include "io.h"

int cmd_vm(char tok[][TOK_W], int ntok);
int cmd_alloc(char tok[][TOK_W], int ntok);
int cmd_rest(char tok[][TOK_W], int ntok);

#endif
