#include "net/net_ip.h"

#include "bytes.h"
#include "checksum.h"

#include <stdio.h>
#include <string.h>

int net_ip_parse(const char *text, uint32_t *addr) {
    unsigned char raw[4];
    if (text == NULL || addr == NULL || !parse_ipv4(text, raw)) {
        return -1;
    }
    *addr = read_be32(raw);
    return 0;
}

int net_ip_fmt(uint32_t addr, char *out, int cap) {
    unsigned char raw[4];
    if (out == NULL || cap < 16) {
        return -1;
    }
    write_be32(raw, addr);
    format_ipv4(raw, out, cap);
    return 0;
}

uint32_t net_ip_mask(int prefix) {
    if (prefix <= 0) {
        return 0;
    }
    if (prefix >= 32) {
        return 0xffffffffu;
    }
    return 0xffffffffu << (32 - prefix);
}

uint32_t net_ip_net(uint32_t addr, int prefix) {
    return addr & net_ip_mask(prefix);
}

uint32_t net_ip_bcast(uint32_t addr, int prefix) {
    return net_ip_net(addr, prefix) | ~net_ip_mask(prefix);
}

int net_ip_cidr(const char *text, uint32_t *net, int *prefix) {
    char buf[32];
    char *slash;
    int pre = 0;
    uint32_t addr = 0;
    if (text == NULL || net == NULL || prefix == NULL || strlen(text) >= sizeof buf) {
        return -1;
    }
    memcpy(buf, text, strlen(text) + 1);
    slash = strchr(buf, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    if (net_ip_parse(buf, &addr) != 0 || sscanf(slash + 1, "%d", &pre) != 1 || pre < 0 || pre > 32) {
        return -1;
    }
    *prefix = pre;
    *net = net_ip_net(addr, pre);
    return 0;
}

int net_ip_hosts(int prefix) {
    int host;
    if (prefix < 0 || prefix > 32) {
        return -1;
    }
    if (prefix == 32) {
        return 1;
    }
    host = 32 - prefix;
    if (host == 1) {
        return 0;
    }
    if (host > 30) {
        return -1;
    }
    return (1 << host) - 2;
}

int net_ip_class(uint32_t addr) {
    unsigned top = addr >> 24;
    if (top >= 1 && top <= 126) {
        return 'A';
    }
    if (top >= 128 && top <= 191) {
        return 'B';
    }
    if (top >= 192 && top <= 223) {
        return 'C';
    }
    if (top >= 224 && top <= 239) {
        return 'D';
    }
    if (top >= 240) {
        return 'E';
    }
    return 0;
}

int net_ip_private(uint32_t addr) {
    if ((addr & 0xff000000u) == 0x0a000000u) {
        return 1;
    }
    if ((addr & 0xfff00000u) == 0xac100000u) {
        return 1;
    }
    if ((addr & 0xffff0000u) == 0xc0a80000u) {
        return 1;
    }
    return 0;
}

int net_ip_fixed(int base_pre, int sub_pre, int *each) {
    int bits;
    int hosts;
    if (each == NULL || sub_pre < base_pre || sub_pre > 32 || base_pre < 0) {
        return -1;
    }
    bits = sub_pre - base_pre;
    if (bits > 30) {
        return -1;
    }
    hosts = net_ip_hosts(sub_pre);
    if (hosts < 0) {
        return -1;
    }
    *each = hosts;
    return 1 << bits;
}

static int common_prefix(uint32_t lo, uint32_t hi) {
    uint32_t diff = lo ^ hi;
    int prefix = 0;
    int bit;
    if (diff == 0) {
        return 32;
    }
    for (bit = 31; bit >= 0; bit--) {
        if ((diff >> bit) & 1u) {
            break;
        }
        prefix++;
    }
    return prefix;
}

int net_ip_agg(const uint32_t *net, const int *pre, int n, uint32_t *out, int *out_pre, int *filled) {
    uint32_t lo;
    uint32_t hi;
    unsigned long long cover = 0;
    unsigned long long size;
    int i;
    int prefix;
    if (net == NULL || pre == NULL || out == NULL || out_pre == NULL || filled == NULL || n <= 0) {
        return -1;
    }
    lo = net[0];
    hi = net[0] | ~net_ip_mask(pre[0]);
    for (i = 0; i < n; i++) {
        uint32_t last = net[i] | ~net_ip_mask(pre[i]);
        if (pre[i] < 0 || pre[i] > 32) {
            return -1;
        }
        if (net[i] < lo) {
            lo = net[i];
        }
        if (last > hi) {
            hi = last;
        }
        cover += 1ull << (32 - pre[i]);
    }
    prefix = common_prefix(lo, hi);
    *out_pre = prefix;
    *out = lo & net_ip_mask(prefix);
    size = 1ull << (32 - prefix);
    *filled = cover == size;
    return 0;
}

int net_ip_vlsm(uint32_t base, int base_pre, int blocks, uint32_t *nets, int *pres, int cap) {
    uint32_t cursor;
    int i;
    int prefix = base_pre;
    if (nets == NULL || pres == NULL || blocks < 2 || blocks > cap || base_pre < 0 || base_pre > 28) {
        return -1;
    }
    cursor = base & net_ip_mask(base_pre);
    for (i = 0; i < blocks - 1; i++) {
        prefix = base_pre + i + 1;
        if (prefix > 30) {
            return -1;
        }
        nets[i] = cursor;
        pres[i] = prefix;
        cursor += 1u << (32 - prefix);
    }
    nets[blocks - 1] = cursor;
    pres[blocks - 1] = prefix;
    return net_ip_hosts(prefix);
}

static int overlap(uint32_t a, int ap, uint32_t b, int bp) {
    unsigned long long a_end;
    unsigned long long b_end;
    unsigned long long a_size = ap >= 32 ? 1ull : 1ull << (32 - ap);
    unsigned long long b_size = bp >= 32 ? 1ull : 1ull << (32 - bp);
    if (ap < 0 || bp < 0 || ap > 32 || bp > 32) {
        return 1;
    }
    a_end = (unsigned long long)a + a_size;
    b_end = (unsigned long long)b + b_size;
    return (unsigned long long)a < b_end && (unsigned long long)b < a_end;
}

static int inside(uint32_t addr, uint32_t net, int prefix) {
    return (addr & net_ip_mask(prefix)) == (net & net_ip_mask(prefix));
}

static int one_hole(uint32_t cover, int cpre, int fine, uint32_t known, int kp, uint32_t cand, int cp,
                    uint32_t *hole) {
    int slots = 1 << (fine - cpre);
    uint32_t step = 1u << (32 - fine);
    int holes = 0;
    int i;
    for (i = 0; i < slots; i++) {
        uint32_t addr = cover + (uint32_t)i * step;
        if (!inside(addr, known, kp) && !inside(addr, cand, cp)) {
            holes++;
            *hole = addr;
        }
    }
    return holes;
}

int net_ip_third(uint32_t known, int kp, uint32_t cand, int cp, int want, uint32_t *other, int *op,
                 int *ok) {
    uint32_t nets[2];
    int pres[2];
    uint32_t cover = 0;
    uint32_t hole = 0;
    int cpre = 0;
    int filled = 0;
    int fine;
    int holes;
    if (other == NULL || op == NULL || ok == NULL) {
        return -1;
    }
    *ok = 0;
    known = net_ip_net(known, kp);
    cand = net_ip_net(cand, cp);
    if (overlap(known, kp, cand, cp)) {
        return 0;
    }
    nets[0] = known;
    nets[1] = cand;
    pres[0] = kp;
    pres[1] = cp;
    if (net_ip_agg(nets, pres, 2, &cover, &cpre, &filled) != 0) {
        return -1;
    }
    fine = kp > cp ? kp : cp;
    if (fine < cpre || fine - cpre > 8) {
        return -1;
    }
    holes = one_hole(cover, cpre, fine, known, kp, cand, cp, &hole);
    if (holes == 1 && want == 3) {
        *other = hole;
        *op = fine;
        *ok = 1;
        return 0;
    }
    if (holes == 0 && want == 3) {
        *other = cover ^ (1u << (32 - cpre));
        *op = cpre;
        *ok = 1;
    }
    return 0;
}

static int emit_piece(NetFrag *item, int data, int hdr, int offset, int mf) {
    item->data = data;
    item->total = data + hdr;
    item->offset = offset / 8;
    item->mf = mf;
    return 0;
}

int net_ip_frag(int total, int hdr, int mtu, int df, NetFrag *out, int cap) {
    int data;
    int max_data;
    int count;
    int left;
    int offset = 0;
    int i;
    if (out == NULL || cap <= 0 || hdr < 20 || mtu <= hdr || total < hdr) {
        return -1;
    }
    data = total - hdr;
    max_data = ((mtu - hdr) / 8) * 8;
    if (max_data <= 0) {
        return -1;
    }
    if (data > max_data && df) {
        return -2;
    }
    if (data <= max_data) {
        emit_piece(&out[0], data, hdr, 0, 0);
        return 1;
    }
    count = (data + max_data - 1) / max_data;
    if (count > cap) {
        return -1;
    }
    left = data;
    for (i = 0; i < count; i++) {
        int piece = left > max_data ? max_data : left;
        emit_piece(&out[i], piece, hdr, offset, i + 1 < count);
        left -= piece;
        offset += piece;
    }
    return count;
}

unsigned net_ip_cksum(const unsigned char *hdr, int len) {
    if (hdr == NULL || len < 20 || len > 60) {
        return 0;
    }
    return ip_header_checksum(hdr, len);
}

int net_ip_set(unsigned char hdr[20], int ttl, int change_src, uint32_t src) {
    if (hdr == NULL || ttl < 0 || ttl > 255) {
        return -1;
    }
    hdr[8] = (unsigned char)ttl;
    if (change_src) {
        write_be32(hdr + 12, src);
    }
    write_be16(hdr + 10, (unsigned short)net_ip_cksum(hdr, 20));
    return 0;
}

static void push_id(int *list, int *n, int id) {
    list[*n] = id;
    (*n)++;
}

int net_ip_2012(int sent[4], int *ns, int hands[4], int *nh, int pads[4], int *np, int *app,
                int *routers) {
    static const uint32_t host = 0xc0a80008u;
    static const struct {
        uint32_t src;
        uint32_t dst;
        int total;
        int ttl;
        int syn;
        int ack;
        uint32_t ackno;
    } row[] = {
        {0xc0a80008u, 0xd3444750u, 48, 128, 1, 0, 0},
        {0xd3444750u, 0xc0a80008u, 48, 49, 1, 1, 0x846b41c6u},
        {0xc0a80008u, 0xd3444750u, 40, 128, 0, 1, 0xe0599ff0u},
        {0xc0a80008u, 0xd3444750u, 56, 128, 0, 1, 0xe0599ff0u},
        {0xd3444750u, 0xc0a80008u, 40, 49, 0, 1, 0x846b41d6u},
    };
    uint32_t max_ack = 0;
    int i;
    if (sent == NULL || hands == NULL || pads == NULL || ns == NULL || nh == NULL || np == NULL) {
        return -1;
    }
    *ns = *nh = *np = 0;
    for (i = 0; i < 5; i++) {
        if (row[i].src == host) {
            push_id(sent, ns, i + 1);
        }
        if (row[i].total < 46) {
            push_id(pads, np, i + 1);
        }
        if (row[i].dst == host && row[i].ack && row[i].ackno > max_ack) {
            max_ack = row[i].ackno;
        }
    }
    push_id(hands, nh, 1);
    push_id(hands, nh, 2);
    push_id(hands, nh, 3);
    if (app != NULL) {
        *app = (int)(max_ack - 0x846b41c6u);
    }
    if (routers != NULL) {
        *routers = 64 - row[4].ttl;
    }
    return row[0].syn && row[1].syn && row[1].ack && row[2].ack ? 0 : -1;
}
