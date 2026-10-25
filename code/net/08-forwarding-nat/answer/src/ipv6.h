#ifndef IPV6_H
#define IPV6_H

#include <stdint.h>

typedef struct {
    int start;
    int len;
    int ties;
} ZeroSpan;

int ipv6_parse(const char *text, uint16_t group[8]);
void ipv6_zero_span(const uint16_t group[8], ZeroSpan *span);
int ipv6_compress(const uint16_t group[8], char *out, int cap);
int ipv6_expand(const uint16_t group[8], char *out, int cap);

#endif
