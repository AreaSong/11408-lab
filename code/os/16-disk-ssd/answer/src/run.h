#ifndef RUN_H
#define RUN_H

#include "ftl.h"
#include "line.h"

int cmd_dispatch(Ftl *ftl, char tok[][TOK_W], int ntok);

#endif
