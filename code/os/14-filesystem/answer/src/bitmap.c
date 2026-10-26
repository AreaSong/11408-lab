#include "bitmap.h"

int bitmap_size(uint64_t disk_bytes, uint64_t block_bytes, uint64_t *blocks,
                uint64_t *map_bytes, uint64_t *map_blocks) {
    uint64_t n;
    uint64_t bytes;

    if (blocks == 0 || map_bytes == 0 || map_blocks == 0 || block_bytes == 0) {
        return -1;
    }
    if (disk_bytes % block_bytes != 0) {
        return -1;
    }
    n = disk_bytes / block_bytes;
    bytes = (n + 7) / 8;
    *blocks = n;
    *map_bytes = bytes;
    *map_blocks = (bytes + block_bytes - 1) / block_bytes;
    return 0;
}

int bitmap_locate(uint64_t block, int word_bits, int origin, uint64_t *word, uint64_t *bit) {
    if (word == 0 || bit == 0 || word_bits < 1) {
        return -1;
    }
    if (origin == 0) {
        *word = block / (uint64_t)word_bits;
        *bit = block % (uint64_t)word_bits;
        return 0;
    }
    if (origin != 1 || block == 0) {
        return -1;
    }
    /* 字号、位号、盘块号都从 1 开始：b = (n-1)*D + m。 */
    *word = (block - 1) / (uint64_t)word_bits + 1;
    *bit = (block - 1) % (uint64_t)word_bits + 1;
    return 0;
}

int bitmap_compose(uint64_t word, uint64_t bit, int word_bits, int origin, uint64_t *block) {
    if (block == 0 || word_bits < 1) {
        return -1;
    }
    if (origin == 0) {
        if (bit >= (uint64_t)word_bits) {
            return -1;
        }
        if (word > UINT64_MAX / (uint64_t)word_bits) {
            return -1;
        }
        *block = word * (uint64_t)word_bits + bit;
        return 0;
    }
    if (origin != 1 || word == 0 || bit == 0 || bit > (uint64_t)word_bits) {
        return -1;
    }
    if (word - 1 > UINT64_MAX / (uint64_t)word_bits) {
        return -1;
    }
    *block = (word - 1) * (uint64_t)word_bits + bit;
    return 0;
}
