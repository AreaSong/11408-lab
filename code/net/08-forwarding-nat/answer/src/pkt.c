#include "pkt.h"

#include "cksum.h"

#include <stdio.h>
#include <string.h>

static unsigned read16(const unsigned char *b) {
    return ((unsigned)b[0] << 8) | b[1];
}

static uint32_t read32(const unsigned char *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static int set_err(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return 0;
}

int ip_load(const unsigned char *bytes, int n, IpView *view, char *err, int err_cap) {
    int ihl;
    unsigned got;
    unsigned calc;

    memset(view, 0, sizeof *view);
    if (n < 20) {
        return set_err(err, err_cap, "至少要有 20 字节才能读 IPv4 首部");
    }
    if ((bytes[0] >> 4) != 4) {
        return set_err(err, err_cap, "版本不是 4");
    }
    ihl = bytes[0] & 0x0f;
    view->hdr_len = ihl * 4;
    if (ihl < 5 || view->hdr_len > n || view->hdr_len > IP_HDR_MAX) {
        return set_err(err, err_cap, "IHL 至少为 5，而且给出的字节要盖住首部");
    }
    view->total = read16(bytes + 2);
    view->ttl = bytes[8];
    view->protocol = bytes[9];
    view->checksum = read16(bytes + 10);
    view->src = read32(bytes + 12);
    view->dst = read32(bytes + 16);
    if (view->total < (unsigned)view->hdr_len) {
        return set_err(err, err_cap, "总长度比首部还短");
    }
    calc = ip_checksum(bytes, view->hdr_len);
    got = view->checksum;
    if (calc != got) {
        return set_err(err, err_cap, "首部校验和与重算不一致");
    }
    memcpy(view->raw, bytes, (size_t)view->hdr_len);
    return 1;
}

static void write32(unsigned char *b, uint32_t value) {
    b[0] = (unsigned char)(value >> 24);
    b[1] = (unsigned char)(value >> 16);
    b[2] = (unsigned char)(value >> 8);
    b[3] = (unsigned char)value;
}

int ip_forward(const IpView *in, int do_nat, uint32_t nat_src, FwdResult *out) {
    unsigned sum;

    memset(out, 0, sizeof *out);
    out->old_ttl = in->ttl;
    out->old_sum = in->checksum;
    out->old_src = in->src;
    out->new_src = do_nat ? nat_src : in->src;
    out->dst = in->dst;
    out->total = in->total;
    out->hdr_len = in->hdr_len;
    out->nat = do_nat;
    if (in->ttl <= 1) {
        out->dropped = 1;
        out->new_ttl = 0;
        return 1;
    }
    memcpy(out->raw, in->raw, (size_t)in->hdr_len);
    out->raw[8] = (unsigned char)(in->ttl - 1);
    out->new_ttl = in->ttl - 1;
    if (do_nat) {
        write32(out->raw + 12, nat_src);
    }
    sum = ip_checksum(out->raw, in->hdr_len);
    out->raw[10] = (unsigned char)(sum >> 8);
    out->raw[11] = (unsigned char)sum;
    out->new_sum = sum;
    return 1;
}
