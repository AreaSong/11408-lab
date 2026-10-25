#ifndef ADDR_H
#define ADDR_H

#include <stdint.h>

int ip_parse(const char *text, uint32_t *out);
void ip_fmt(uint32_t ip, char *out, int cap);
int prefix_parse(const char *text, uint32_t *net, int *prefix);
void prefix_fmt(uint32_t net, int prefix, char *out, int cap);

/* 前缀 0 的掩码是 0。移位 32 位在 C 里无定义，所以 0 和 32 单独算。 */
uint32_t prefix_mask(int prefix);
int ip_aligned(uint32_t addr, int prefix);

int mac_parse(const char *text, uint8_t mac[6]);
void mac_fmt(const uint8_t mac[6], char *out, int cap);
void bits8(unsigned value, char out[9]);

#endif
