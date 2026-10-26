#ifndef RUN_H
#define RUN_H

#include "alloc.h"
#include "line.h"
#include "ring.h"
#include "spool.h"

int cmd_dispatch(IoSys *io, Spool *sp, Ring *rg, char tok[][TOK_W], int ntok);

#endif
