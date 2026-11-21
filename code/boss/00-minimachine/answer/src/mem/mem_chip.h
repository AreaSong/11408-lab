#ifndef MEM_CHIP_H
#define MEM_CHIP_H

#include <stdint.h>
#include <stdio.h>

/* mode：0 位扩展，1 字扩展，2 字位同时扩展，3 不用扩展。 */
enum { MEM_EXT_BIT = 0, MEM_EXT_WORD = 1, MEM_EXT_BOTH = 2, MEM_EXT_NONE = 3 };

typedef struct {
    uint32_t units; /* 目标单元数。按字节编址时就是字节数 */
    int word_bits;
    uint32_t chip_units;
    int chip_bits;
} MemChipReq;

typedef struct {
    int groups;
    int per_group;
    int chips;
    int chip_addr_bits;
    int select_bits;
    int addr_bits;
    int mode;
} MemChipPlan;

typedef struct {
    int group;
    uint32_t min_addr;
    uint32_t max_addr;
    uint32_t in_chip;
    uint32_t select;
} MemChipAt;

int mem_chip_plan(const MemChipReq *req, MemChipPlan *out);
int mem_chip_locate(uint32_t units, uint32_t chip_units, uint32_t addr, MemChipAt *out);
void mem_chip_report(FILE *fp, const MemChipReq *req);
void mem_chip_locate_report(FILE *fp, uint32_t units, uint32_t chip_units, uint32_t addr);

#endif
