#ifndef FS_FREE_H
#define FS_FREE_H

#include <stdint.h>

#define FS_GROUP_CAP 100
#define FS_GROUP_DISK 4096

typedef struct {
    int cap;
    int n;
    int blk[FS_GROUP_CAP];
    unsigned char used[FS_GROUP_DISK];
    unsigned char dn[FS_GROUP_DISK];
    int db[FS_GROUP_DISK][FS_GROUP_CAP];
} FsGroup;

int fs_bitmap_size(uint64_t disk_bytes, uint64_t block_bytes, uint64_t *blocks, uint64_t *map_bytes,
                   uint64_t *map_blocks);
int fs_bitmap_locate(uint64_t block, int word_bits, int origin, uint64_t *word, uint64_t *bit);
int fs_bitmap_compose(uint64_t word, uint64_t bit, int word_bits, int origin, uint64_t *block);

void fs_group_init(FsGroup *g, int cap);
int fs_group_fill(FsGroup *g, int block, int count, int first, int step);
int fs_group_set_stack(FsGroup *g, const int *blk, int n);
int fs_group_total(const FsGroup *g);
int fs_group_alloc(FsGroup *g, int *block, int *did_read);
int fs_group_free(FsGroup *g, int block, int *did_write);

#endif
