#ifndef ETHFRAME_H
#define ETHFRAME_H

#include "macaddr.h"

#define ETH_DATA_MAX 1500
#define ETH_RAW_MAX 1522

typedef struct {
    unsigned char dst[MAC_LEN];
    unsigned char src[MAC_LEN];
    int is_len;
    unsigned field;
    const unsigned char *data;
    int data_len;
    int vlan;
    int pri;
    int cfi;
} EthSpec;

typedef struct {
    int is_len;
    unsigned field;
    int data_len;
    int pad;
    int vlan;
    int pri;
    int cfi;
    unsigned fcs;
    unsigned char raw[ETH_RAW_MAX];
    int raw_len;
} EthFrame;

int eth_build(const EthSpec *in, EthFrame *out, char *err);

#endif
