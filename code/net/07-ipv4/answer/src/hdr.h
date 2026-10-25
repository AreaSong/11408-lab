#ifndef HDR_H
#define HDR_H

#include <stdint.h>

#define IP_HDR_MIN 20
#define IP_CAPTURE_MAX 80
#define ETH_PAYLOAD_MIN 46

typedef struct {
    int version;
    int ihl;
    int hdr_len;
    unsigned tos;
    unsigned total_len;
    unsigned id;
    int reserved;
    int df;
    int mf;
    unsigned frag_off;
    unsigned flags_word;
    unsigned ttl;
    unsigned protocol;
    unsigned checksum;
    unsigned checksum_calc;
    int checksum_ok;
    uint32_t src;
    uint32_t dst;
    int has_tcp;
    unsigned src_port;
    unsigned dst_port;
    uint32_t seq;
    uint32_t ack;
    unsigned tcp_off;
    unsigned tcp_flags;
    int tcp_short;
} IpInfo;

/* n 可以小于总长度：2012 的表只给了前 40 字节。 */
int ip_parse_header(const unsigned char *b, int n, IpInfo *out, char *err, int err_cap);

#endif
