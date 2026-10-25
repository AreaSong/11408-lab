#include "cksum.h"

unsigned ip_checksum(const unsigned char *bytes, int len) {
    unsigned sum = 0;
    int i;

    for (i = 0; i + 1 < len; i += 2) {
        if (i == 10) {
            continue;
        }
        sum += ((unsigned)bytes[i] << 8) | bytes[i + 1];
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (~sum) & 0xffffu;
}
