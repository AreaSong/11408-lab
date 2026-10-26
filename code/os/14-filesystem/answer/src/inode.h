#ifndef INODE_H
#define INODE_H

#include <stdint.h>

typedef struct {
    uint64_t block_size;
    uint64_t addr_size;
    int direct;
    int single;
    int doubly;
    int triply;
} InodeSpec;

typedef struct {
    uint64_t ptrs;
    uint64_t blocks;
    uint64_t bytes;
    uint64_t level_blocks[4];
    uint64_t level_bytes[4];
} InodeMax;

typedef struct {
    int level;
    uint64_t logical;
    int data_reads;
    int addr_reads;
} InodeHit;

typedef struct {
    uint64_t blocks;
    int level;
    int data_reads;
    int addr_reads;
    char indirect[16];
} InodeCover;

int inode_max(const InodeSpec *spec, InodeMax *out);
int inode_expr(const InodeMax *max, char *buf, int cap);
int inode_hit(const InodeSpec *spec, uint64_t offset, InodeHit *hit);
int inode_cover(const InodeSpec *spec, uint64_t file_bytes, InodeCover *cover);
const char *inode_level_name(int level);

#endif
