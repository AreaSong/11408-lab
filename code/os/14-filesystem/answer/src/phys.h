#ifndef PHYS_H
#define PHYS_H

#include <stdint.h>

enum {
    PHYS_CONT = 0,
    PHYS_LINK = 1,
    PHYS_FAT = 2,
    PHYS_FAT_DISK = 3,
    PHYS_INDEX = 4
};

typedef struct {
    int reads;
    int move;
    int head;
} InsertInfo;

int phys_kind(const char *name);
const char *phys_name(int kind);
int phys_visit(int kind, int k, int *reads, int *next_reads);
int insert_cont(int n, int pos, InsertInfo *info);
int insert_link(int n, int pos, int *reads);
int link_max(uint64_t block_bytes, uint64_t ptr_bytes, uint64_t *file_bytes);
int fat_limit(int entry_bytes, uint64_t cluster, uint64_t *fat_bytes, uint64_t *file_bytes);
int fat_nth(uint64_t cluster, uint64_t byte_no, const int *chain, int nchain,
            int *nth, int *cluster_no);

#endif
