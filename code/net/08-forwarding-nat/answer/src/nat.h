#ifndef NAT_H
#define NAT_H

#include <stdint.h>

#define NAT_MAX 32
#define NAT_SLOTS 128
#define NAT_BOXES 4

enum { NAT_TCP = 6, NAT_UDP = 17, NAT_NONE = 0, NAT_BASIC = 1, NAT_POOL = 2 };

typedef struct {
    int proto;
    uint32_t in_ip;
    uint32_t out_ip;
    unsigned in_port;
    unsigned out_port;
} NatEntry;

typedef struct {
    int used;
    int proto;
    uint32_t ip;
    unsigned port;
    int index;
} KeySlot;

typedef struct {
    char name[8];
    int live;
    int mode;
    uint32_t public_ip;
    unsigned pool_lo;
    unsigned pool_hi;
    unsigned pool_next;
    NatEntry entry[NAT_MAX];
    int nentry;
    KeySlot in_key[NAT_SLOTS];
    KeySlot out_key[NAT_SLOTS];
} NatBox;

typedef struct {
    int proto;
    uint32_t src;
    uint32_t dst;
    unsigned sport;
    unsigned dport;
    int created;
} Flow;

void natbox_init(NatBox *box, const char *name);
int natbox_map(NatBox *box, int proto, uint32_t pub, unsigned pp, uint32_t pri, unsigned prip, char *err,
               int err_cap);
int natbox_basic(NatBox *box, uint32_t pub, char *err, int err_cap);
int natbox_pool(NatBox *box, uint32_t pub, unsigned lo, unsigned hi, char *err, int err_cap);
int natbox_out(NatBox *box, Flow *flow, char *err, int err_cap);
int natbox_in(NatBox *box, Flow *flow, char *err, int err_cap);

#endif
