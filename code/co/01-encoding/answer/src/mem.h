#ifndef MEM_H
#define MEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define MEM_MAX_MEMBER 16
#define MEM_NAME 32

/* 卷面 32 位模型：long 是 4 字节。double 按王道按 8 对齐。 */

typedef struct {
    char name[MEM_NAME];
    char type_name[16];
    char label[MEM_NAME];
    int size;
    int align;
    int offset;
    int pad_before;
    bool has_value;
    unsigned long long machine;
} Member;

bool mem_parse_member(const char *line, Member *m, bool *is_end, char *err, size_t errn);
int mem_layout(Member *m, int count, int *max_align, int *total);
int mem_byte(unsigned long long machine, int size, bool little, int index);

int mem_print_endian(FILE *out, const char *mode, unsigned long long addr, int nbytes, const char *pattern,
                     bool has_query, unsigned long long query);
int mem_print_align(FILE *out, unsigned long long base, bool little, int index, bool index_set, int find_byte,
                    bool find_set, Member *m, int count);

#endif
