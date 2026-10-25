#include "macaddr.h"

#include "scan.h"

#include <stdio.h>
#include <string.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int mac_parse(const char *text, unsigned char mac[MAC_LEN], char *err) {
    int i;

    if (text == NULL) {
        return lab_fail(err, "缺少 MAC 地址");
    }
    for (i = 0; i < MAC_LEN; i++) {
        int hi = hex_val(text[i * 3]);
        int lo = hex_val(text[i * 3 + 1]);

        if (hi < 0 || lo < 0) {
            return lab_fail(err, "MAC 地址写成 00-11-22-33-44-55");
        }
        if (i < MAC_LEN - 1 && text[i * 3 + 2] != '-') {
            return lab_fail(err, "MAC 地址的字节之间用 - 分开");
        }
        mac[i] = (unsigned char)((hi << 4) | lo);
    }
    if (text[MAC_LEN * 3 - 1] != '\0') {
        return lab_fail(err, "MAC 地址写成 00-11-22-33-44-55");
    }
    return 1;
}

void mac_fmt(const unsigned char mac[MAC_LEN], char out[MAC_TEXT]) {
    snprintf(out, MAC_TEXT, "%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
}

int mac_eq(const unsigned char a[MAC_LEN], const unsigned char b[MAC_LEN]) {
    return memcmp(a, b, MAC_LEN) == 0;
}

/* 首字节最低位是 I/G。1 表示组地址，广播地址也落在这里。 */
int mac_is_group(const unsigned char mac[MAC_LEN]) {
    return (mac[0] & 0x01) != 0;
}

int mac_is_broadcast(const unsigned char mac[MAC_LEN]) {
    int i;

    for (i = 0; i < MAC_LEN; i++) {
        if (mac[i] != 0xff) {
            return 0;
        }
    }
    return 1;
}
