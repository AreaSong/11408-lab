#ifndef FS_INDEX_H
#define FS_INDEX_H

#include <stdint.h>

typedef struct {
    uint64_t block_size;
    uint64_t addr_size;
    int direct;
    int single;
    int doubly;
    int triply;
} FsInodeSpec;

typedef struct {
    uint64_t ptrs;
    uint64_t blocks;
    uint64_t bytes;
    uint64_t level_blocks[4];
    uint64_t level_bytes[4];
} FsInodeMax;

typedef struct {
    int level;
    uint64_t logical;
    int data_reads;
    int addr_reads;
} FsInodeHit;

typedef struct {
    uint64_t blocks;
    int level;
    int data_reads;
    int addr_reads;
    char indirect[16];
} FsInodeCover;

typedef struct {
    int reads;
    int move;
    int head;
} FsInsert;

enum {
    FS_PHYS_CONT = 0,
    FS_PHYS_LINK = 1,
    FS_PHYS_FAT = 2,
    FS_PHYS_FAT_DISK = 3,
    FS_PHYS_INDEX = 4
};

int fs_inode_max(const FsInodeSpec *spec, FsInodeMax *out);
int fs_inode_expr(const FsInodeMax *max, char *buf, int cap);
int fs_inode_hit(const FsInodeSpec *spec, uint64_t offset, FsInodeHit *hit);
int fs_inode_cover(const FsInodeSpec *spec, uint64_t file_bytes, FsInodeCover *cover);
const char *fs_inode_level(int level);

int fs_phys_kind(const char *name);
const char *fs_phys_name(int kind);
int fs_phys_visit(int kind, int k, int *reads, int *next_reads);
int fs_insert_cont(int n, int pos, FsInsert *info);
int fs_insert_link(int n, int pos, int *reads);
int fs_link_max(uint64_t block_bytes, uint64_t ptr_bytes, uint64_t *file_bytes);
int fs_fat_limit(int entry_bytes, uint64_t cluster, uint64_t *fat_bytes, uint64_t *file_bytes);
int fs_fat_nth(uint64_t cluster, uint64_t byte_no, const int *chain, int nchain, int *nth,
               int *cluster_no);

#endif
