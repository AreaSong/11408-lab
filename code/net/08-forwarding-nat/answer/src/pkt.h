#ifndef PKT_H
#define PKT_H

#include <stdint.h>

#define IP_HDR_MAX 60

typedef struct {
    int hdr_len;
    unsigned total;
    unsigned ttl;
    unsigned protocol;
    unsigned checksum;
    uint32_t src;
    uint32_t dst;
    unsigned char raw[IP_HDR_MAX];
} IpView;

/* 校验和不对、版本不是 4、首部不完整时返回 0。 */
int ip_load(const unsigned char *bytes, int n, IpView *view, char *err, int err_cap);

typedef struct {
    int dropped;
    int nat;
    unsigned old_ttl;
    unsigned new_ttl;
    unsigned old_sum;
    unsigned new_sum;
    uint32_t old_src;
    uint32_t new_src;
    uint32_t dst;
    unsigned total;
    int hdr_len;
    unsigned char raw[IP_HDR_MAX];
} FwdResult;

/* TTL 减到 0 时 dropped = 1，不再改首部。否则写出新的 TTL、可选的新源地址和校验和。 */
int ip_forward(const IpView *in, int do_nat, uint32_t nat_src, FwdResult *out);

#endif
