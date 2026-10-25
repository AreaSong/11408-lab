#include "cap.h"

#include "scan.h"

#include <string.h>

#define TCP_SYN 0x02u
#define TCP_ACK 0x10u

static int find_syn(const IpInfo *pkt, int n, uint32_t host, int *index) {
    int i;
    int found = -1;

    for (i = 0; i < n; i++) {
        if (!pkt[i].has_tcp || pkt[i].src != host) {
            continue;
        }
        if ((pkt[i].tcp_flags & TCP_SYN) != 0 && (pkt[i].tcp_flags & TCP_ACK) == 0) {
            if (found >= 0) {
                *index = -2;
                return 0;
            }
            found = i;
        }
    }
    *index = found;
    return found >= 0;
}

static int find_synack(const IpInfo *pkt, int n, uint32_t server, uint32_t ack, int *index) {
    int i;

    for (i = 0; i < n; i++) {
        if (!pkt[i].has_tcp || pkt[i].src != server) {
            continue;
        }
        if ((pkt[i].tcp_flags & TCP_SYN) != 0 && (pkt[i].tcp_flags & TCP_ACK) != 0 &&
            pkt[i].ack == ack) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

static int find_third(const IpInfo *pkt, int n, uint32_t host, uint32_t seq, uint32_t ack, int *index) {
    int i;

    for (i = 0; i < n; i++) {
        if (!pkt[i].has_tcp || pkt[i].src != host) {
            continue;
        }
        if ((pkt[i].tcp_flags & TCP_SYN) == 0 && (pkt[i].tcp_flags & TCP_ACK) != 0 && pkt[i].seq == seq &&
            pkt[i].ack == ack) {
            *index = i;
            return 1;
        }
    }
    return 0;
}

static void collect_ends(const IpInfo *pkt, int n, uint32_t host, CapResult *out) {
    int i;

    for (i = 0; i < n; i++) {
        if (pkt[i].src == host) {
            out->from_host[out->n_host++] = i + 1;
        }
        if (pkt[i].total_len < ETH_PAYLOAD_MIN) {
            out->padded[out->n_pad++] = i + 1;
        }
    }
}

static int app_bytes(const IpInfo *pkt, int n, uint32_t server, uint32_t data_seq, CapResult *out,
                     char *err, int err_cap) {
    int seen = 0;
    uint32_t max_ack = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (pkt[i].has_tcp && pkt[i].src == server && (!seen || pkt[i].ack >= max_ack)) {
            max_ack = pkt[i].ack;
            seen = 1;
        }
    }
    if (!seen || max_ack < data_seq) {
        return fail_msg(err, err_cap, "服务器的确认号对不上");
    }
    out->data_seq = data_seq;
    out->max_ack = max_ack;
    out->app_bytes = (int)(max_ack - data_seq);
    return 1;
}

static int match_origin(const IpInfo *pkt, int n, const IpInfo *origin, CapResult *out, char *err,
                        int err_cap) {
    int hit = -1;
    int i;

    for (i = 0; i < n; i++) {
        if (pkt[i].id != origin->id) {
            continue;
        }
        if (hit >= 0) {
            return fail_msg(err, err_cap, "标识相同的捕获分组不止一个");
        }
        hit = i;
    }
    if (hit < 0) {
        return fail_msg(err, err_cap, "捕获里没有这个标识");
    }
    if (origin->ttl < pkt[hit].ttl) {
        return fail_msg(err, err_cap, "生存时间变大了");
    }
    out->routers = (int)(origin->ttl - pkt[hit].ttl);
    out->origin_ttl = origin->ttl;
    out->arrive_ttl = pkt[hit].ttl;
    out->origin_dst = origin->dst;
    out->arrive_dst = pkt[hit].dst;
    out->dst_changed = origin->dst != pkt[hit].dst;
    out->matched_id = origin->id;
    return 1;
}

int cap_conclude(const IpInfo *pkt, int n, uint32_t host, uint32_t server, const IpInfo *origin,
                 CapResult *out, char *err, int err_cap) {
    int syn = -1;
    int synack = -1;
    int third = -1;

    memset(out, 0, sizeof *out);
    out->routers = -1;
    if (n < 1 || n > CAP_MAX) {
        return fail_msg(err, err_cap, "捕获的分组数要在 1 到 8");
    }
    collect_ends(pkt, n, host, out);
    if (!find_syn(pkt, n, host, &syn)) {
        return fail_msg(err, err_cap, syn == -2 ? "客户的 SYN 不止一个" : "没有找到客户的 SYN");
    }
    if (!find_synack(pkt, n, server, pkt[syn].seq + 1u, &synack) ||
        !find_third(pkt, n, host, pkt[syn].seq + 1u, pkt[synack].seq + 1u, &third)) {
        return fail_msg(err, err_cap, "三次握手对不上");
    }
    out->hand[0] = syn + 1;
    out->hand[1] = synack + 1;
    out->hand[2] = third + 1;
    out->n_hand = 3;
    if (!app_bytes(pkt, n, server, pkt[syn].seq + 1u, out, err, err_cap)) {
        return 0;
    }
    if (origin != NULL && !match_origin(pkt, n, origin, out, err, err_cap)) {
        return 0;
    }
    return 1;
}
