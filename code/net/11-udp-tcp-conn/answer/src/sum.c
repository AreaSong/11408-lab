#include "sum.h"

unsigned int sum_add_bytes(unsigned int sum, const unsigned char *p, int len) {
    int i = 0;

    while (i + 1 < len) {
        sum += ((unsigned int)p[i] << 8) | p[i + 1];
        i += 2;
    }
    /* 网络序是大端：剩下的单字节是高 8 位，低 8 位补 0。补的字节不传输。 */
    if (i < len) {
        sum += (unsigned int)p[i] << 8;
    }
    return sum;
}

unsigned int sum_fold(unsigned int sum) {
    while (sum >> 16) {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return sum;
}

unsigned short sum_finish(unsigned int sum, int zero_as_ffff) {
    unsigned int inv = (~sum_fold(sum)) & 0xffffu;

    /*
     * UDP 校验和字段的 0 表示「发送方没算」。真算出 0 时必须改记 0xffff，
     * 否则接收方会跳过检验。TCP 没有这个可选含义，第 01 关也同样改记。
     */
    if (zero_as_ffff && inv == 0) {
        inv = 0xffffu;
    }
    return (unsigned short)inv;
}
