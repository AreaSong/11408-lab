#ifndef CAP_H
#define CAP_H

#include "hdr.h"

#define CAP_MAX 8

typedef struct {
    int from_host[CAP_MAX];
    int n_host;
    int hand[3];
    int n_hand;
    int padded[CAP_MAX];
    int n_pad;
    int app_bytes;
    uint32_t data_seq;
    uint32_t max_ack;
    int routers;
    int dst_changed;
    uint32_t origin_dst;
    uint32_t arrive_dst;
    unsigned origin_ttl;
    unsigned arrive_ttl;
    unsigned matched_id;
} CapResult;

int cap_conclude(const IpInfo *pkt, int n, uint32_t host, uint32_t server, const IpInfo *origin,
                 CapResult *out, char *err, int err_cap);

#endif
