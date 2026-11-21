#ifndef NET_NIC_H
#define NET_NIC_H

#include "common/mm_machine.h"

#include <stdio.h>
#include <stdint.h>

/* 阶段 10 才接上。没接上时 50H～53H 仍只是端口数组。 */
#define NET_PORT_CMD 0x50u
#define NET_PORT_ADDR 0x51u
#define NET_PORT_LEN 0x52u
#define NET_PORT_STAT 0x53u

int net_on(MmMachine *m);
void net_drop(MmMachine *m);
void net_after_reset(MmMachine *m);
int net_put(MmMachine *m, uint32_t addr, const unsigned char *bytes, int n);
int net_corrupt(MmMachine *m, int on);
int net_arm_tx(MmMachine *m, uint32_t addr, int nbytes);
int net_pump(MmMachine *m);
uint32_t net_stat(const MmMachine *m);
void net_show(const MmMachine *m, FILE *out);

#endif
