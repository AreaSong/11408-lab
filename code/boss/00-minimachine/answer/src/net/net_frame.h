#ifndef NET_FRAME_H
#define NET_FRAME_H

#include <stdint.h>

int net_bit_stuff(const char *in, char *out, int cap);
int net_byte_stuff(const char *hex, char *out, int cap);
int net_crc_rem(const char *bits, const char *gen, char *rem, int cap, int *clean);
int net_ham_need(int data_bits);
int net_ham_flip(const char *data, int pos, int *bad, char *fixed, int fcap, char *raw, int rcap);
int net_eth_pad(int data_len, int vlan, int *frame, int *pad);
uint16_t net_crc16(const uint8_t *bytes, int n);
/* ber_ppm 是百万分之一。1000000 表示这一位必翻，0 表示不翻。种子固定，结果可重复。 */
int net_ber_flips(uint32_t seed, int ber_ppm, int bits, int trials);

#endif
