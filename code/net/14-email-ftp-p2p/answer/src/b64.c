#include "b64.h"

#include <stddef.h>

static const char *ABC = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int emit4(char *out, int cap, int o, int a, int b, int c, int d) {
    if (o + 4 >= cap) {
        return -1;
    }
    out[o] = ABC[a];
    out[o + 1] = ABC[b];
    out[o + 2] = c < 0 ? '=' : ABC[c];
    out[o + 3] = d < 0 ? '=' : ABC[d];
    return o + 4;
}

int b64_encode(const unsigned char *in, int n, char *out, int cap) {
    int i = 0;
    int o = 0;

    if (in == NULL || out == NULL || n < 0 || cap <= 0) {
        return -1;
    }
    while (i + 3 <= n) {
        unsigned int v = ((unsigned int)in[i] << 16) | ((unsigned int)in[i + 1] << 8) | in[i + 2];
        o = emit4(out, cap, o, (v >> 18) & 63, (v >> 12) & 63, (v >> 6) & 63, v & 63);
        if (o < 0) {
            return -1;
        }
        i += 3;
    }
    if (i < n) {
        unsigned int v = (unsigned int)in[i] << 16;
        int c = -1;
        int d = -1;
        if (i + 1 < n) {
            v |= (unsigned int)in[i + 1] << 8;
            c = (v >> 6) & 63;
        }
        o = emit4(out, cap, o, (v >> 18) & 63, (v >> 12) & 63, c, d);
        if (o < 0) {
            return -1;
        }
    }
    out[o] = '\0';
    return o;
}

static int val(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

static int push_byte(unsigned char *out, int cap, int o, int b) {
    if (o >= cap) {
        return -1;
    }
    out[o] = (unsigned char)b;
    return o + 1;
}

int b64_decode(const char *in, unsigned char *out, int cap) {
    int quad[4];
    int k = 0;
    int o = 0;
    int pad = 0;

    if (in == NULL || out == NULL || cap < 0) {
        return -1;
    }
    for (; *in != '\0'; in++) {
        int v;
        if (*in == ' ' || *in == '\n' || *in == '\r' || *in == '\t') {
            continue;
        }
        if (*in == '=') {
            v = 0;
            pad++;
        } else {
            v = val(*in);
            if (v < 0 || pad > 0) {
                return -1;
            }
        }
        quad[k++] = v;
        if (k < 4) {
            continue;
        }
        o = push_byte(out, cap, o, (quad[0] << 2) | (quad[1] >> 4));
        if (o < 0) {
            return -1;
        }
        if (pad < 2) {
            o = push_byte(out, cap, o, ((quad[1] & 15) << 4) | (quad[2] >> 2));
            if (o < 0) {
                return -1;
            }
        }
        if (pad < 1) {
            o = push_byte(out, cap, o, ((quad[2] & 3) << 6) | quad[3]);
            if (o < 0) {
                return -1;
            }
        }
        k = 0;
    }
    if (k != 0) {
        return -1;
    }
    return o;
}
