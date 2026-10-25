#ifndef MACTAB_H
#define MACTAB_H

#include "macaddr.h"

#define MACTAB_CAP 32

typedef struct {
    unsigned char mac[MAC_LEN];
    int vlan;
    int port;
    int used;
} MacSlot;

typedef struct {
    MacSlot slot[MACTAB_CAP];
    int count;
} MacTab;

typedef struct {
    unsigned char mac[MAC_LEN];
    int vlan;
    int port;
} MacRow;

void mactab_init(MacTab *t);

/* 1 新写入，2 端口没变，3 从别的端口挪过来，0 表满。old_port 只在返回 3 时有意义。 */
int mactab_learn(MacTab *t, const unsigned char mac[MAC_LEN], int vlan, int port,
                 int *old_port);
int mactab_find(const MacTab *t, const unsigned char mac[MAC_LEN], int vlan, int *port);
int mactab_rows(const MacTab *t, MacRow *rows, int cap);

#endif
