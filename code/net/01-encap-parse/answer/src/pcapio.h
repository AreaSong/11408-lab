#ifndef PCAPIO_H
#define PCAPIO_H

#include <stdbool.h>
#include <stdio.h>

#define PCAP_BYTES_MAX 20000

typedef struct {
    unsigned int magic;
    int little;
    int nanos;
    unsigned short major;
    unsigned short minor;
    int thiszone;
    unsigned int sigfigs;
    unsigned int snaplen;
    unsigned int network;
    int count;
} PcapView;

/* 写成这台小端机器上 tcpdump -w 用的经典 pcap：魔数落到文件里是 d4 c3 b2 a1。 */
bool pcap_write_one(unsigned char *out, int cap, unsigned int ts_sec, unsigned int ts_sub,
                    const unsigned char *frame, int frame_len, int *written);

bool pcap_view(const unsigned char *buf, int len, PcapView *view, char *err, int err_cap);

bool pcap_packet(const unsigned char *buf, int len, int index, unsigned int *ts_sec,
                 unsigned int *ts_sub, int *incl_len, int *orig_len,
                 const unsigned char **data, char *err, int err_cap);

bool pcap_report(FILE *fp, const unsigned char *buf, int len, char *err, int err_cap);

#endif
