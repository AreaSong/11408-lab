#ifndef TRACE_H
#define TRACE_H

#include "cache.h"

#include <stdio.h>

int trace_show_header(FILE *out, const Fields *f, int replace, int write_back, int write_alloc);
int trace_show_access(FILE *out, int index, int is_write, uint64_t addr, const Cache *c,
                      const AccessInfo *info);
int trace_show_summary(FILE *out, const Cache *c);

#endif
