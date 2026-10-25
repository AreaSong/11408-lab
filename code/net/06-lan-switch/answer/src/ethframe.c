#include "ethframe.h"

#include "scan.h"

#include <string.h>

static unsigned crc32_eth(const unsigned char *p, int n) {
    unsigned crc = 0xffffffffu;
    int i;
    int b;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (b = 0; b < 8; b++) {
            unsigned bit = crc & 1u;
            crc >>= 1;
            if (bit) {
                crc ^= 0xedb88320u;
            }
        }
    }
    return ~crc;
}

static int check_field(const EthSpec *in, char *err) {
    if (in->data_len < 0 || in->data_len > ETH_DATA_MAX) {
        return lab_fail(err, "数据字段是 0～1500 字节");
    }
    if (in->is_len) {
        if (in->field > 1500 || in->field != (unsigned)in->data_len) {
            return lab_fail(err, "802.3 的长度字段要等于数据字节数，并且不超过 1500");
        }
        return 1;
    }
    if (in->field < 0x0600u) {
        return lab_fail(err, "类型要大于等于 0x0600；小于等于 1500 请改用 len");
    }
    if (in->field == 0x8100u) {
        return lab_fail(err, "0x8100 是 VLAN 标签的 TPID，请用 vlan 而不是把它当成类型");
    }
    return 1;
}

static int check_vlan(const EthSpec *in, char *err) {
    if (in->vlan == 0) {
        return 1;
    }
    if (in->vlan < 1 || in->vlan > 4094) {
        return lab_fail(err, "VLAN 号用 1～4094，0 和 4095 保留");
    }
    if (in->pri < 0 || in->pri > 7 || in->cfi < 0 || in->cfi > 1) {
        return lab_fail(err, "优先级是 0～7，CFI 是 0 或 1");
    }
    return 1;
}

static void put_u16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static int write_header(const EthSpec *in, EthFrame *out) {
    int n = 0;

    memcpy(out->raw, in->dst, MAC_LEN);
    memcpy(out->raw + MAC_LEN, in->src, MAC_LEN);
    n = 12;
    if (in->vlan != 0) {
        unsigned tci = ((unsigned)in->pri << 13) | ((unsigned)in->cfi << 12) | (unsigned)in->vlan;
        put_u16(out->raw + n, 0x8100u);
        put_u16(out->raw + n + 2, tci);
        n += 4;
    }
    put_u16(out->raw + n, in->field);
    return n + 2;
}

static void write_fcs(EthFrame *out, int n) {
    unsigned fcs = crc32_eth(out->raw, n);

    out->fcs = fcs;
    out->raw[n] = (unsigned char)fcs;
    out->raw[n + 1] = (unsigned char)(fcs >> 8);
    out->raw[n + 2] = (unsigned char)(fcs >> 16);
    out->raw[n + 3] = (unsigned char)(fcs >> 24);
    out->raw_len = n + 4;
}

int eth_build(const EthSpec *in, EthFrame *out, char *err) {
    int n;
    int pad;
    int i;

    if (!check_field(in, err) || !check_vlan(in, err)) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    out->is_len = in->is_len;
    out->field = in->field;
    out->data_len = in->data_len;
    out->vlan = in->vlan;
    out->pri = in->pri;
    out->cfi = in->cfi;
    n = write_header(in, out);
    if (in->data_len > 0 && in->data != NULL) {
        memcpy(out->raw + n, in->data, (size_t)in->data_len);
    }
    n += in->data_len;
    /* 数据字段最少 46 字节，标签不占用这 46 字节。不够就用 0 填，FCS 把填充也算进去。 */
    pad = in->data_len < 46 ? 46 - in->data_len : 0;
    for (i = 0; i < pad; i++) {
        out->raw[n++] = 0;
    }
    out->pad = pad;
    write_fcs(out, n);
    return 1;
}
