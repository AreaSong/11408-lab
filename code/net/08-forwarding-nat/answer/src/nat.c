#include "nat.h"

#include <stdio.h>
#include <string.h>

void natbox_init(NatBox *box, const char *name) {
    memset(box, 0, sizeof *box);
    snprintf(box->name, sizeof box->name, "%s", name);
    box->live = 1;
}

static int set_err(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return 0;
}

static unsigned key_hash(int proto, uint32_t ip, unsigned port) {
    unsigned h = (unsigned)proto * 131u;

    h ^= ip * 16777619u;
    h ^= port * 17u;
    return h % NAT_SLOTS;
}

static int key_find(const KeySlot *tab, int proto, uint32_t ip, unsigned port) {
    unsigned h = key_hash(proto, ip, port);
    int i;

    for (i = 0; i < NAT_SLOTS; i++) {
        int idx = (int)((h + (unsigned)i) % NAT_SLOTS);

        if (!tab[idx].used) {
            return -1;
        }
        if (tab[idx].proto == proto && tab[idx].ip == ip && tab[idx].port == port) {
            return tab[idx].index;
        }
    }
    return -1;
}

static int key_put(KeySlot *tab, int proto, uint32_t ip, unsigned port, int index) {
    unsigned h = key_hash(proto, ip, port);
    int i;

    if (key_find(tab, proto, ip, port) >= 0) {
        return 0;
    }
    for (i = 0; i < NAT_SLOTS; i++) {
        int idx = (int)((h + (unsigned)i) % NAT_SLOTS);

        if (!tab[idx].used) {
            tab[idx].used = 1;
            tab[idx].proto = proto;
            tab[idx].ip = ip;
            tab[idx].port = port;
            tab[idx].index = index;
            return 1;
        }
    }
    return 0;
}

static int add_entry(NatBox *box, int proto, uint32_t in_ip, unsigned in_port, uint32_t out_ip,
                     unsigned out_port, char *err, int err_cap) {
    NatEntry *entry;
    int index;

    if (box->nentry >= NAT_MAX) {
        return set_err(err, err_cap, "NAT 表最多 32 条");
    }
    if (key_find(box->out_key, proto, out_ip, out_port) >= 0) {
        return set_err(err, err_cap, "公网地址和端口已经被占用");
    }
    if (key_find(box->in_key, proto, in_ip, in_port) >= 0) {
        return set_err(err, err_cap, "内网地址和端口已经有映射");
    }
    index = box->nentry++;
    entry = &box->entry[index];
    entry->proto = proto;
    entry->in_ip = in_ip;
    entry->in_port = in_port;
    entry->out_ip = out_ip;
    entry->out_port = out_port;
    if (!key_put(box->in_key, proto, in_ip, in_port, index) ||
        !key_put(box->out_key, proto, out_ip, out_port, index)) {
        return set_err(err, err_cap, "NAT 散列没有空位");
    }
    return 1;
}

int natbox_map(NatBox *box, int proto, uint32_t pub, unsigned pp, uint32_t pri, unsigned prip, char *err,
               int err_cap) {
    if (pp == 0 || prip == 0 || pp > 65535u || prip > 65535u) {
        return set_err(err, err_cap, "端口要在 1 到 65535");
    }
    return add_entry(box, proto, pri, prip, pub, pp, err, err_cap);
}

int natbox_basic(NatBox *box, uint32_t pub, char *err, int err_cap) {
    if (box->mode != NAT_NONE) {
        return set_err(err, err_cap, "这台路由器已经设过 basic 或 pool");
    }
    if (pub == 0) {
        return set_err(err, err_cap, "公网地址不能是 0.0.0.0");
    }
    box->mode = NAT_BASIC;
    box->public_ip = pub;
    return 1;
}

int natbox_pool(NatBox *box, uint32_t pub, unsigned lo, unsigned hi, char *err, int err_cap) {
    if (box->mode != NAT_NONE) {
        return set_err(err, err_cap, "这台路由器已经设过 basic 或 pool");
    }
    if (pub == 0 || lo == 0 || hi > 65535u || lo > hi) {
        return set_err(err, err_cap, "端口池要落在 1 到 65535，而且起点不能大于终点");
    }
    box->mode = NAT_POOL;
    box->public_ip = pub;
    box->pool_lo = lo;
    box->pool_hi = hi;
    box->pool_next = lo;
    return 1;
}

static int alloc_port(NatBox *box, int proto, unsigned *port) {
    unsigned p = box->pool_next;

    if (p < box->pool_lo) {
        p = box->pool_lo;
    }
    while (p <= box->pool_hi) {
        if (key_find(box->out_key, proto, box->public_ip, p) < 0) {
            box->pool_next = (p == 65535u) ? 65536u : p + 1;
            *port = p;
            return 1;
        }
        if (p == 65535u) {
            break;
        }
        p++;
    }
    return 0;
}

int natbox_out(NatBox *box, Flow *flow, char *err, int err_cap) {
    int index;
    unsigned port;

    index = key_find(box->in_key, flow->proto, flow->src, flow->sport);
    if (index >= 0) {
        flow->src = box->entry[index].out_ip;
        flow->sport = box->entry[index].out_port;
        flow->created = 0;
        return 1;
    }
    if (box->mode == NAT_POOL) {
        if (!alloc_port(box, flow->proto, &port)) {
            return set_err(err, err_cap, "端口池用完了");
        }
    } else if (box->mode == NAT_BASIC) {
        port = flow->sport;
    } else {
        return set_err(err, err_cap, "出站转换要先设 basic 或 pool");
    }
    if (!add_entry(box, flow->proto, flow->src, flow->sport, box->public_ip, port, err, err_cap)) {
        return 0;
    }
    flow->src = box->public_ip;
    flow->sport = port;
    flow->created = 1;
    return 1;
}

int natbox_in(NatBox *box, Flow *flow, char *err, int err_cap) {
    int index = key_find(box->out_key, flow->proto, flow->dst, flow->dport);

    if (index < 0) {
        return set_err(err, err_cap, "没有这条公网地址和端口的映射");
    }
    flow->dst = box->entry[index].in_ip;
    flow->dport = box->entry[index].in_port;
    flow->created = 0;
    return 1;
}
