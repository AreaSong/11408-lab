#include "checksum.h"

#include <string.h>

#include "bytes.h"

unsigned int checksum_add(unsigned int sum, const unsigned char *p, int len) {
    int i = 0;

    while (i + 1 < len) {
        sum += read_be16(p + i);
        i += 2;
    }
    /* 奇数长度时，最后一个字节是这一字的高 8 位，低 8 位补 0。 */
    if (i < len) {
        sum += (unsigned int)p[i] << 8;
    }
    return sum;
}

unsigned int checksum_fold(unsigned int sum) {
    while (sum >> 16) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return sum;
}

unsigned short ip_header_checksum(const unsigned char *hdr, int hdr_len) {
    unsigned char tmp[60];
    unsigned int sum;

    if (hdr_len < 20 || hdr_len > 60) {
        return 0;
    }
    memcpy(tmp, hdr, (size_t)hdr_len);
    tmp[10] = 0;
    tmp[11] = 0;
    sum = checksum_fold(checksum_add(0, tmp, hdr_len));
    return (unsigned short)(~sum & 0xffffu);
}

unsigned short tcp_checksum(const unsigned char src[4], const unsigned char dst[4],
                            const unsigned char *segment, int seg_len) {
    unsigned char pseudo[12];
    unsigned char tmp[1600];
    unsigned int sum;

    if (seg_len < 20 || seg_len > (int)sizeof tmp) {
        return 0;
    }
    memcpy(pseudo, src, 4);
    memcpy(pseudo + 4, dst, 4);
    pseudo[8] = 0;
    pseudo[9] = 6;
    write_be16(pseudo + 10, (unsigned short)seg_len);
    memcpy(tmp, segment, (size_t)seg_len);
    tmp[16] = 0;
    tmp[17] = 0;
    sum = checksum_add(0, pseudo, 12);
    sum = checksum_add(sum, tmp, seg_len);
    sum = ~checksum_fold(sum) & 0xffffu;
    if (sum == 0) {
        sum = 0xffffu;
    }
    return (unsigned short)sum;
}
