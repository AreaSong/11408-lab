#include "phys.h"

#include <string.h>

int phys_kind(const char *name) {
    if (strcmp(name, "cont") == 0) {
        return PHYS_CONT;
    }
    if (strcmp(name, "link") == 0) {
        return PHYS_LINK;
    }
    if (strcmp(name, "fat") == 0) {
        return PHYS_FAT;
    }
    if (strcmp(name, "fat-disk") == 0) {
        return PHYS_FAT_DISK;
    }
    if (strcmp(name, "index") == 0) {
        return PHYS_INDEX;
    }
    return -1;
}

const char *phys_name(int kind) {
    static const char *names[] = {"cont", "link", "fat", "fat-disk", "index"};
    if (kind < 0 || kind > PHYS_INDEX) {
        return "?";
    }
    return names[kind];
}

int phys_visit(int kind, int k, int *reads, int *next_reads) {
    if (k < 1 || reads == 0 || next_reads == 0) {
        return -1;
    }
    /* 紧接着再读下一块时，上一块的指针、内存里的 FAT、刚读入的索引块都还在。 */
    *next_reads = 1;
    switch (kind) {
    case PHYS_CONT:
        *reads = 1;
        return 0;
    case PHYS_LINK:
        /* 逻辑块从 1 计。指针嵌在块里，读第 k 块要先把第 1..k 块都读进来。 */
        *reads = k;
        return 0;
    case PHYS_FAT:
        *reads = 1;
        return 0;
    case PHYS_FAT_DISK:
        /* 整张 FAT 还在磁盘上时先整表读入，算 1 次，再读数据块。 */
        *reads = 2;
        return 0;
    case PHYS_INDEX:
        /* 单级索引块一开始不在内存。多级索引用 inode，不走这里。 */
        *reads = 2;
        return 0;
    default:
        return -1;
    }
}

int insert_cont(int n, int pos, InsertInfo *info) {
    int head;
    int tail;

    if (info == 0 || n < 1 || pos < 1 || pos > n) {
        return -1;
    }
    /* 插成第 pos 条。前移 pos-1 条，后移 n-pos+1 条，取搬得少的那一边。 */
    head = pos - 1;
    tail = n - pos + 1;
    if (head <= tail) {
        info->move = head;
        info->head = 1;
    } else {
        info->move = tail;
        info->head = 0;
    }
    info->reads = info->move * 2 + 1;
    return 0;
}

int insert_link(int n, int pos, int *reads) {
    if (reads == 0 || n < 1 || pos < 1 || pos > n) {
        return -1;
    }
    if (pos == 1) {
        /* 前面没有块。新块成为开头，FCB 已在内存，改起始块号不再访盘。 */
        *reads = 1;
        return 0;
    }
    /* 读到第 pos-1 块，写出新块，再把改过指针的第 pos-1 块写回。 */
    *reads = (pos - 1) + 2;
    return 0;
}

int link_max(uint64_t block_bytes, uint64_t ptr_bytes, uint64_t *file_bytes) {
    uint64_t blocks;
    uint64_t data;

    if (file_bytes == 0 || ptr_bytes == 0 || ptr_bytes > 4 || block_bytes <= ptr_bytes) {
        return -1;
    }
    /* 指针占 ptr 字节，能点名 2^(8*ptr) 个块。块里要扣掉指针本身。 */
    blocks = 1ull << (8u * (unsigned)ptr_bytes);
    data = block_bytes - ptr_bytes;
    if (blocks != 0 && data > UINT64_MAX / blocks) {
        return -1;
    }
    *file_bytes = blocks * data;
    return 0;
}

int fat_limit(int entry_bytes, uint64_t cluster, uint64_t *fat_bytes, uint64_t *file_bytes) {
    uint64_t entries;
    int bits;

    if (fat_bytes == 0 || file_bytes == 0 || cluster == 0) {
        return -1;
    }
    if (entry_bytes != 1 && entry_bytes != 2 && entry_bytes != 4) {
        return -1;
    }
    bits = entry_bytes * 8;
    entries = 1ull << (unsigned)bits;
    if (entries > UINT64_MAX / (uint64_t)entry_bytes || entries > UINT64_MAX / cluster) {
        return -1;
    }
    *fat_bytes = entries * (uint64_t)entry_bytes;
    *file_bytes = entries * cluster;
    return 0;
}

int fat_nth(uint64_t cluster, uint64_t byte_no, const int *chain, int nchain,
            int *nth, int *cluster_no) {
    uint64_t index;

    if (chain == 0 || nth == 0 || cluster_no == 0 || nchain < 1 || cluster == 0 || byte_no == 0) {
        return -1;
    }
    /* 「第 byte_no 个字节」从 1 计，落在第 index 个逻辑簇（从 0 计）。 */
    index = (byte_no - 1) / cluster;
    if (index >= (uint64_t)nchain) {
        return -1;
    }
    *nth = (int)index + 1;
    *cluster_no = chain[index];
    return 0;
}
