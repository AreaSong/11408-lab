#include "net/net_frame.h"

#include <stdio.h>
#include <string.h>

static int bit_ok(char c) { return c == '0' || c == '1'; }

int net_bit_stuff(const char *in, char *out, int cap) {
    int ones = 0;
    int o = 0;
    int i;
    int n;
    if (in == NULL || out == NULL || cap < 2) {
        return -1;
    }
    n = (int)strlen(in);
    for (i = 0; i < n; i++) {
        if (!bit_ok(in[i]) || o + 2 >= cap) {
            return -1;
        }
        out[o++] = in[i];
        ones = in[i] == '1' ? ones + 1 : 0;
        /* HDLC：连续 5 个 1 后面立刻插 0，标志 01111110 才不会出现在数据里。 */
        if (ones == 5) {
            out[o++] = '0';
            ones = 0;
        }
    }
    out[o] = '\0';
    return 0;
}

static int hex_byte(const char *s, unsigned *out) {
    unsigned v = 0;
    int i;
    for (i = 0; i < 2; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v |= (unsigned)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            v |= (unsigned)(c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            v |= (unsigned)(c - 'a' + 10);
        } else {
            return -1;
        }
    }
    *out = v;
    return 0;
}

static int append_hex(char *out, int cap, int *o, unsigned byte) {
    int n = snprintf(out + *o, (size_t)(cap - *o), "%s%02X", *o == 0 ? "" : " ", byte);
    if (n < 0 || *o + n >= cap) {
        return -1;
    }
    *o += n;
    return 0;
}

int net_byte_stuff(const char *hex, char *out, int cap) {
    const char *p = hex;
    int o = 0;
    if (hex == NULL || out == NULL || cap < 3) {
        return -1;
    }
    while (*p != '\0') {
        unsigned byte = 0;
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (hex_byte(p, &byte) != 0) {
            return -1;
        }
        p += 2;
        /* PPP：7E、7D 和小于 20H 的控制字符都写成 7D 再跟「原字节异或 20H」。 */
        if (byte == 0x7E || byte == 0x7D || byte < 0x20) {
            if (append_hex(out, cap, &o, 0x7D) != 0 || append_hex(out, cap, &o, byte ^ 0x20) != 0) {
                return -1;
            }
        } else if (append_hex(out, cap, &o, byte) != 0) {
            return -1;
        }
    }
    out[o] = '\0';
    return o == 0 ? -1 : 0;
}

int net_crc_rem(const char *bits, const char *gen, char *rem, int cap, int *clean) {
    char buf[256];
    int n;
    int g;
    int r;
    int i;
    int j;
    if (bits == NULL || gen == NULL || rem == NULL) {
        return -1;
    }
    n = (int)strlen(bits);
    g = (int)strlen(gen);
    r = g - 1;
    if (n < r || r < 1 || n >= (int)sizeof buf || cap < r + 1 || gen[0] != '1' || gen[r] != '1') {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (!bit_ok(bits[i])) {
            return -1;
        }
        buf[i] = bits[i];
    }
    for (i = 0; i < g; i++) {
        if (!bit_ok(gen[i])) {
            return -1;
        }
    }
    for (i = 0; i <= n - g; i++) {
        if (buf[i] != '1') {
            continue;
        }
        for (j = 0; j < g; j++) {
            buf[i + j] = (buf[i + j] == gen[j]) ? '0' : '1';
        }
    }
    if (clean != NULL) {
        *clean = 1;
    }
    for (i = 0; i < r; i++) {
        rem[i] = buf[n - r + i];
        if (clean != NULL && rem[i] != '0') {
            *clean = 0;
        }
    }
    rem[r] = '\0';
    return 0;
}

int net_ham_need(int data_bits) {
    int r = 0;
    if (data_bits < 1 || data_bits > 4096) {
        return -1;
    }
    while ((1 << r) < data_bits + r + 1) {
        r++;
        if (r > 20) {
            return -1;
        }
    }
    return r;
}

static int pow2(int x) { return x > 0 && (x & (x - 1)) == 0; }

static int ham_place(const char *data, char *code, int n) {
    int i;
    int di = 0;
    int m = (int)strlen(data);
    for (i = 1; i <= n; i++) {
        code[i] = '0';
    }
    for (i = 1; i <= n; i++) {
        if (pow2(i)) {
            continue;
        }
        if (di >= m || !bit_ok(data[di])) {
            return -1;
        }
        code[i] = data[di++];
    }
    return di == m ? 0 : -1;
}

static void ham_parity(char *code, int n) {
    int p;
    for (p = 1; p <= n; p <<= 1) {
        int parity = 0;
        int i;
        for (i = 1; i <= n; i++) {
            if ((i & p) != 0 && code[i] == '1') {
                parity ^= 1;
            }
        }
        code[p] = parity ? '1' : '0';
    }
}

static int ham_syn(const char *code, int n) {
    int syn = 0;
    int p;
    for (p = 1; p <= n; p <<= 1) {
        int parity = 0;
        int i;
        for (i = 1; i <= n; i++) {
            if ((i & p) != 0 && code[i] == '1') {
                parity ^= 1;
            }
        }
        if (parity) {
            syn |= p;
        }
    }
    return syn;
}

static void ham_data(const char *code, int n, char *raw, int rcap) {
    int i;
    int o = 0;
    for (i = 1; i <= n && o + 1 < rcap; i++) {
        if (!pow2(i)) {
            raw[o++] = code[i];
        }
    }
    raw[o] = '\0';
}

int net_ham_flip(const char *data, int pos, int *bad, char *fixed, int fcap, char *raw, int rcap) {
    char code[66];
    int m;
    int r;
    int n;
    int syn;
    int i;
    if (data == NULL || bad == NULL || fixed == NULL || raw == NULL) {
        return -1;
    }
    m = (int)strlen(data);
    r = net_ham_need(m);
    n = m + r;
    if (r < 0 || n > 64 || pos < 1 || pos > n || fcap < n + 1 || rcap < m + 1 || ham_place(data, code, n) != 0) {
        return -1;
    }
    ham_parity(code, n);
    code[pos] = code[pos] == '1' ? '0' : '1';
    syn = ham_syn(code, n);
    *bad = syn;
    if (syn >= 1 && syn <= n) {
        code[syn] = code[syn] == '1' ? '0' : '1';
    }
    for (i = 1; i <= n; i++) {
        fixed[i - 1] = code[i];
    }
    fixed[n] = '\0';
    ham_data(code, n, raw, rcap);
    return 0;
}

int net_eth_pad(int data_len, int vlan, int *frame, int *pad) {
    int min_data = 46;
    int overhead = vlan ? 22 : 18;
    int used;
    if (frame == NULL || pad == NULL || data_len < 0 || data_len > 1500) {
        return -1;
    }
    /* 802.1Q 多 4 字节标签，最短帧从 64 变成 68，数据仍然补到 46 字节。 */
    *pad = data_len < min_data ? min_data - data_len : 0;
    used = overhead + data_len + *pad;
    *frame = used < (vlan ? 68 : 64) ? (vlan ? 68 : 64) : used;
    return 0;
}

uint16_t net_crc16(const uint8_t *bytes, int n) {
    uint16_t crc = 0xFFFF;
    int i;
    int b;
    if (bytes == NULL || n < 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (b = 0; b < 8; b++) {
            if ((crc & 0x8000) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

int net_ber_flips(uint32_t seed, int ber_ppm, int bits, int trials) {
    uint32_t x = seed;
    int flips = 0;
    int t;
    int i;
    if (ber_ppm < 0 || ber_ppm > 1000000 || bits < 1 || trials < 1 || trials > 1000) {
        return -1;
    }
    for (t = 0; t < trials; t++) {
        for (i = 0; i < bits; i++) {
            x = x * 1103515245u + 12345u;
            if ((int)(x % 1000000u) < ber_ppm) {
                flips++;
            }
        }
    }
    return flips;
}
