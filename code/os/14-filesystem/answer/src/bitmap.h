#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>

int bitmap_size(uint64_t disk_bytes, uint64_t block_bytes, uint64_t *blocks,
                uint64_t *map_bytes, uint64_t *map_blocks);
int bitmap_locate(uint64_t block, int word_bits, int origin, uint64_t *word, uint64_t *bit);
int bitmap_compose(uint64_t word, uint64_t bit, int word_bits, int origin, uint64_t *block);

#endif
