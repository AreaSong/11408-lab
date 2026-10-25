#ifndef PACKET_H
#define PACKET_H

#include <stdbool.h>

#define ETH_ADDR_LEN 6
#define ETH_HDR_LEN 14
#define ETH_MIN_PAYLOAD 46
#define ETH_FCS_LEN 4
#define IP_ADDR_LEN 4
#define IP_HDR_MIN 20
#define TCP_HDR_MIN 20
#define PAYLOAD_MAX 1500
#define FRAME_MAX 1600

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV6 0x86DD
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

typedef struct {
    unsigned char dst[ETH_ADDR_LEN];
    unsigned char src[ETH_ADDR_LEN];
    unsigned short type;
} EthHeader;

typedef struct {
    unsigned char version;
    unsigned char ihl;
    unsigned char tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flags_frag;
    unsigned char ttl;
    unsigned char protocol;
    unsigned short checksum;
    unsigned char src[IP_ADDR_LEN];
    unsigned char dst[IP_ADDR_LEN];
    int hdr_len;
} IpHeader;

typedef struct {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned int seq;
    unsigned int ack;
    unsigned char data_offset;
    unsigned char flags;
    unsigned short window;
    unsigned short checksum;
    unsigned short urgent;
    int hdr_len;
} TcpHeader;

typedef struct {
    EthHeader eth;
    IpHeader ip;
    TcpHeader tcp;
    int has_ip;
    int has_tcp;
    unsigned char payload[FRAME_MAX];
    int payload_len;
    int payload_total;
    int pad_len;
    int fcs_len;
    int given_len;
    int frame_len;
    int implied_no_fcs;
    int ip_checksum_ok;
    int tcp_checksum_ok;
    unsigned char frame[FRAME_MAX];
} Packet;

const char *eth_type_name(unsigned short type);
const char *ip_proto_name(unsigned char protocol);

/* 调用前填好 MAC、IP、TTL、标识、端口、序号、确认号、标志、窗口和 payload。 */
bool packet_build(Packet *pkt, char *err, int err_cap);
bool packet_parse(const unsigned char *buf, int len, Packet *pkt, char *err, int err_cap);

#endif
