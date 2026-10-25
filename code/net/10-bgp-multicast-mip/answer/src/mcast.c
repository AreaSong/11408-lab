#include "mcast.h"

#include <string.h>

int mcast_map(const unsigned char ip[4], unsigned char mac[6]) {
    /* D 类前 4 位是 1110。组标识有 28 位，MAC 只带走低 23 位。 */
    if ((ip[0] & 0xf0) != 0xe0) {
        return 0;
    }
    mac[0] = 0x01;
    mac[1] = 0x00;
    mac[2] = 0x5e;
    /* 第 25 位固定为 0，所以第 2 个字节的最高位不进 MAC。 */
    mac[3] = (unsigned char)(ip[1] & 0x7f);
    mac[4] = ip[2];
    mac[5] = ip[3];
    return 1;
}

void mcast_kin(const unsigned char ip[4], unsigned char hi[4], unsigned char top[4],
               unsigned char other[4]) {
    memcpy(hi, ip, 4);
    memcpy(top, ip, 4);
    memcpy(other, ip, 4);
    hi[1] = (unsigned char)(ip[1] ^ 0x80);
    top[0] = (unsigned char)(224 + (((ip[0] - 224) + 1) & 15));
    other[3] = (unsigned char)(ip[3] ^ 0x01);
}

int mcast_same_count(void) {
    return 1 << 5;
}
