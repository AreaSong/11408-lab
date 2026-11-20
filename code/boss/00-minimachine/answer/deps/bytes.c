#include "bytes.h"

#include <stdlib.h>
#include <string.h>

/*
 * 帧里的多字节整数是网络字节序（大端）：高位放在低地址。
 * 这台机器是小端，不能把 unsigned short 直接叠到字节数组上。
 * htons 在小端上做的是同一件事，单测里和这里对过。
 */
unsigned short read_be16(const unsigned char *p) {
    return (unsigned short)((p[0] << 8) | p[1]);
}

unsigned int read_be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

unsigned short read_le16(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

unsigned int read_le32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

void write_be16(unsigned char *p, unsigned short v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

void write_be32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void write_le16(unsigned char *p, unsigned short v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

void write_le32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

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

int parse_mac(const char *text, unsigned char mac[6]) {
    unsigned char tmp[6];
    int i;
    const char *p = text;

    for (i = 0; i < 6; i++) {
        int hi = hex_val(p[0]);
        int lo = hex_val(p[1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        tmp[i] = (unsigned char)((hi << 4) | lo);
        p += 2;
        if (i == 5) {
            break;
        }
        if (*p != '-' && *p != ':') {
            return 0;
        }
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    memcpy(mac, tmp, 6);
    return 1;
}

int parse_ipv4(const char *text, unsigned char ip[4]) {
    unsigned int a, b, c, d;
    char tail;

    if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
        return 0;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return 0;
    }
    ip[0] = (unsigned char)a;
    ip[1] = (unsigned char)b;
    ip[2] = (unsigned char)c;
    ip[3] = (unsigned char)d;
    return 1;
}

int parse_u32(const char *text, unsigned int *out) {
    char *end = NULL;
    unsigned long v;

    if (text[0] == '\0') {
        return 0;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        v = strtoul(text, &end, 16);
    } else {
        v = strtoul(text, &end, 10);
    }
    if (end == text || *end != '\0' || v > 0xffffffffUL) {
        return 0;
    }
    *out = (unsigned int)v;
    return 1;
}

static int same_flag(const char *text, const char *name) {
    while (*name) {
        char c = *text;
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *name) {
            return 0;
        }
        text++;
        name++;
    }
    return *text == '\0';
}

static int one_flag(const char *text, unsigned char *bit) {
    if (same_flag(text, "fin")) {
        *bit = 0x01;
    } else if (same_flag(text, "syn")) {
        *bit = 0x02;
    } else if (same_flag(text, "rst")) {
        *bit = 0x04;
    } else if (same_flag(text, "psh")) {
        *bit = 0x08;
    } else if (same_flag(text, "ack")) {
        *bit = 0x10;
    } else if (same_flag(text, "urg")) {
        *bit = 0x20;
    } else {
        return 0;
    }
    return 1;
}

int parse_tcp_flags(const char *text, unsigned char *flags) {
    char buf[64];
    char *p;
    unsigned char value = 0;
    size_t n;

    if (strcmp(text, "none") == 0 || strcmp(text, "0") == 0) {
        *flags = 0;
        return 1;
    }
    n = strlen(text);
    if (n == 0 || n >= sizeof buf) {
        return 0;
    }
    memcpy(buf, text, n + 1);
    p = buf;
    while (*p) {
        char *comma = strchr(p, ',');
        unsigned char bit = 0;
        if (comma) {
            *comma = '\0';
        }
        if (!one_flag(p, &bit)) {
            return 0;
        }
        value = (unsigned char)(value | bit);
        if (!comma) {
            break;
        }
        p = comma + 1;
    }
    *flags = value;
    return 1;
}

static int hex_byte(char hi, char lo, unsigned char *out) {
    int h = hex_val(hi);
    int l = hex_val(lo);
    if (h < 0 || l < 0) {
        return 0;
    }
    *out = (unsigned char)((h << 4) | l);
    return 1;
}

int unescape(const char *text, unsigned char *out, int cap, int *out_len,
             char *err, int err_cap) {
    int n = 0;

    while (*text) {
        unsigned char byte = 0;
        if (n >= cap) {
            snprintf(err, err_cap, "应用层报文最长 %d 字节", cap);
            return 0;
        }
        if (*text != '\\') {
            out[n++] = (unsigned char)*text++;
            continue;
        }
        text++;
        if (*text == 'r') {
            byte = '\r';
        } else if (*text == 'n') {
            byte = '\n';
        } else if (*text == 't') {
            byte = '\t';
        } else if (*text == '\\') {
            byte = '\\';
        } else if (*text == 'x') {
            if (!hex_byte(text[1], text[2], &byte)) {
                snprintf(err, err_cap, "\\x 后面要跟两位十六进制");
                return 0;
            }
            text += 2;
        } else {
            snprintf(err, err_cap, "不认识的转义，只能用 \\r \\n \\t \\\\ \\xHH");
            return 0;
        }
        out[n++] = byte;
        text++;
    }
    *out_len = n;
    return 1;
}

void format_mac(const unsigned char mac[6], char *out, int cap) {
    snprintf(out, cap, "%02x-%02x-%02x-%02x-%02x-%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void format_ipv4(const unsigned char ip[4], char *out, int cap) {
    snprintf(out, cap, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static int take_byte(const char **pp, unsigned char *out) {
    const char *p = *pp;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    if (*p == '\0' || *p == '\n') {
        *pp = p;
        return 0;
    }
    if (hex_val(p[0]) < 0 || hex_val(p[1]) < 0 || hex_val(p[2]) >= 0) {
        return -1;
    }
    *out = (unsigned char)((hex_val(p[0]) << 4) | hex_val(p[1]));
    *pp = p + 2;
    return 1;
}

int read_hex_bytes(FILE *fp, unsigned char *out, int n, int *bad_index) {
    char line[8192];
    int got = 0;

    while (got < n) {
        const char *p;
        if (!fgets(line, (int)sizeof line, fp)) {
            *bad_index = got + 1;
            return 0;
        }
        if (strchr(line, '\n') == NULL && !feof(fp)) {
            *bad_index = got + 1;
            return -1;
        }
        p = line;
        while (got < n) {
            unsigned char byte = 0;
            int rc = take_byte(&p, &byte);
            if (rc == 0) {
                break;
            }
            if (rc < 0) {
                *bad_index = got + 1;
                return -1;
            }
            out[got++] = byte;
        }
        if (got == n) {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
                p++;
            }
            if (*p != '\0') {
                *bad_index = n + 1;
                return -1;
            }
        }
    }
    return 1;
}
