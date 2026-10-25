#ifndef MAP_H
#define MAP_H

#include "fields.h"
#include "geom.h"

#include <stdint.h>
#include <stdio.h>

/* 按字节编址的 Cache。行数、路数、块大小的约束和第 06 关相同。 */
int cache_fields(Fields *out, int pa_bits, int n_lines, int block, int n_ways);
int cache_from_bytes(Fields *out, int pa_bits, int cache_bytes, int block, int n_ways);

/* 块号的低位是组号，高位是标记。组号 = 块号 mod 组数。 */
int block_place(const Fields *f, uint64_t block_no, int *set, uint64_t *tag);

int show_cfield(FILE *out, const Fields *f);
int show_band(FILE *out, const PageGeom *g, const TlbGeom *t, const Fields *f);
int show_blk(FILE *out, const Fields *f, uint64_t block_no);
int show_cset(FILE *out, const PageGeom *page, const Fields *f, uint64_t addr, int low_only);

#endif
