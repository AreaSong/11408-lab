#ifndef UDP_H
#define UDP_H

#define UDP_DATA_MAX 256
#define UDP_OK 1
#define UDP_BAD 0
#define UDP_SKIP 2

typedef struct {
    unsigned char src[4];
    unsigned char dst[4];
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short length;
    unsigned short checksum;
    unsigned char data[UDP_DATA_MAX];
    int data_len;
    int partial;
} UdpGram;

const char *udp_set(UdpGram *g, const unsigned char src[4], const unsigned char dst[4],
                    unsigned short src_port, unsigned short dst_port,
                    const unsigned char *data, int data_len);
const char *udp_read(const unsigned char *bytes, int n, UdpGram *g);
unsigned short udp_checksum(const UdpGram *g);
int udp_cover(const UdpGram *g, int zero_checksum, unsigned char *buf, int cap);
int udp_verify(const UdpGram *g);
const char *udp_port_name(unsigned short port);
int udp_eff_tenths(unsigned int app, int tcp, unsigned int *tenths);

#endif
