#ifndef EXPAND_H
#define EXPAND_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint64_t groups;
    int per_group;
    uint64_t chips;
    int chip_addr_bits;
    int select_bits;
    int total_addr_bits;
    int mode;
} ExpandInfo;

typedef struct {
    uint64_t group;
    uint64_t min_addr;
    uint64_t max_addr;
    uint64_t in_chip;
    uint64_t select;
    int chip_bits;
    int select_bits;
} LocateInfo;

typedef struct {
    uint64_t start;
    uint64_t end;
} AddrRange;

int expand_info(uint64_t target_units, int target_bits, uint64_t chip_units, int chip_bits,
                ExpandInfo *out);
int locate_info(uint64_t target_units, uint64_t chip_units, uint64_t addr, LocateInfo *out);
int address_bits(uint64_t bytes);
int zone_rom(int addr_bits, int low_parts, int high_parts, uint64_t *ram_last, uint64_t *rom_first,
             uint64_t *rom_last);
int cs_ranges(int addr_bits, int chip_bits, const char *pattern, AddrRange *ranges, int cap,
              int *count);

int show_expand(FILE *out, uint64_t target_units, int target_bits, uint64_t chip_units,
                int chip_bits);
int show_locate(FILE *out, uint64_t target_units, uint64_t chip_units, uint64_t addr);
int show_mar(FILE *out, uint64_t space_bytes);
int show_zone(FILE *out, int addr_bits, int low_parts, int high_parts);
int show_cs(FILE *out, int addr_bits, int chip_bits, const char *pattern);

#endif
