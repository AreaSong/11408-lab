#ifndef DEV_PORT_H
#define DEV_PORT_H

#include "common/mm_machine.h"

#include <stdio.h>
#include <stdint.h>

/* 阶段 09 才接上。没接上时，这些地址和端口仍只是普通存储。 */
#define DEV_CON_OUT 0xFFFF0000u
#define DEV_CON_STAT 0xFFFF0004u
#define DEV_DISK_CMD 0x20u
#define DEV_DISK_BLOCK 0x21u
#define DEV_DISK_ADDR 0x22u
#define DEV_DISK_STAT 0x23u
#define DEV_PRINT_DATA 0x30u
#define DEV_PRINT_STAT 0x31u
#define DEV_SECTOR 512

int dev_on(MmMachine *m);
int dev_live(const MmMachine *m);
void dev_drop(MmMachine *m);
void dev_after_reset(MmMachine *m);

int dev_disk_fill(MmMachine *m, int block, const unsigned char *bytes, int n);
int dev_dma_arm(MmMachine *m, int block, uint32_t addr, int nbytes, int to_mem);
int dev_after_mem(MmMachine *m);
int dev_dma_left(const MmMachine *m);
int dev_dma_steals(const MmMachine *m);
void dev_dma_show(const MmMachine *m, FILE *out);

int dev_print_byte(MmMachine *m, unsigned byte);
int dev_print_done(MmMachine *m);
int dev_print_len(const MmMachine *m);
int dev_print_at(const MmMachine *m, int index);

/* 处理了存储器映射的控制台就返回 0，不是这块地址返回 -1。 */
int dev_mmio(MmMachine *m, uint32_t addr, int size, uint32_t *data, int writing);

#endif
