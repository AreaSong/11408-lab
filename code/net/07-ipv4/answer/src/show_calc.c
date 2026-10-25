#include "show.h"

#include "ip.h"

static void bits8(unsigned v, char *out) {
    int i;

    for (i = 0; i < 8; i++) {
        out[i] = (char)(((v >> (7 - i)) & 1u) ? '1' : '0');
    }
    out[8] = '\0';
}

static unsigned octet_of(uint32_t ip, int index) {
    return (ip >> (8 * (3 - index))) & 255u;
}

static void print_partial(FILE *fp, uint32_t addr, int prefix) {
    int index;
    unsigned ip_b;
    unsigned mask_b;
    char a[9];
    char m[9];
    char n[9];

    if (prefix <= 0 || prefix >= 32 || prefix % 8 == 0) {
        fprintf(fp, "前缀 /%d 落在字节边界上，网络地址把后面的主机字节清成 0。\n", prefix);
        return;
    }
    index = prefix / 8;
    ip_b = octet_of(addr, index);
    mask_b = octet_of(prefix_mask(prefix), index);
    bits8(ip_b, a);
    bits8(mask_b, m);
    bits8(ip_b & mask_b, n);
    fprintf(fp, "第 %d 字节：地址 %s，掩码 %s，相与 %s = %u\n", index + 1, a, m, n, ip_b & mask_b);
}

static void print_range(FILE *fp, Block b) {
    char first[16];
    char last[16];
    char text[32];

    block_fmt(b, text, (int)sizeof text);
    if (b.prefix >= 31) {
        fprintf(fp, "%s，可分配 %llu 个", text, (unsigned long long)ip_usable(b.prefix));
        return;
    }
    ip_fmt(b.base + 1u, first, (int)sizeof first);
    ip_fmt(block_last(b) - 1u, last, (int)sizeof last);
    fprintf(fp, "%s，可分配 %s～%s（%llu 个）", text, first, last, (unsigned long long)ip_usable(b.prefix));
}

void show_addr(FILE *fp, uint32_t addr, int prefix) {
    char ip[16];
    char mask[16];
    char net[32];
    char bcast[16];
    Block nb;

    nb.base = ip_network(addr, prefix);
    nb.prefix = prefix;
    ip_fmt(addr, ip, (int)sizeof ip);
    ip_fmt(prefix_mask(prefix), mask, (int)sizeof mask);
    block_fmt(nb, net, (int)sizeof net);
    ip_fmt(ip_broadcast(addr, prefix), bcast, (int)sizeof bcast);
    fprintf(fp, "【CIDR】\n");
    fprintf(fp, "地址 %s，前缀 /%d，掩码 %s\n", ip, prefix, mask);
    fprintf(fp, "网络地址 = 地址 AND 掩码。广播地址把主机号设成全 1。\n");
    fprintf(fp, "主机号全 0 是网络本身，全 1 是广播，这两个不能分给主机。\n");
    if (prefix < 31) {
        print_partial(fp, addr, prefix);
    }
    if (prefix >= 32) {
        fprintf(fp, "这是主机路由。网络地址就是这一个地址，可分配 1 个。\n");
        fprintf(fp, "结果：网络 %s，可分配 1 个\n", net);
        return;
    }
    if (prefix == 31) {
        fprintf(fp, "主机号只剩 1 位。王道按全 0、全 1 都不能分配，可分配 0 个。\n");
        fprintf(fp, "点对点链路的 RFC 3021 两个地址都能用。408 计算题按王道。\n");
        fprintf(fp, "结果：网络 %s，广播 %s，可分配 0 个\n", net, bcast);
        return;
    }
    fprintf(fp, "网络地址 %s\n", net);
    fprintf(fp, "广播地址 %s\n", bcast);
    fprintf(fp, "地址共 %llu 个，可分配 %llu 个\n", (unsigned long long)ip_space(prefix),
            (unsigned long long)ip_usable(prefix));
    fprintf(fp, "发往广播地址的分组，这 %llu 台主机都能收到。\n", (unsigned long long)ip_usable(prefix));
    fprintf(fp, "结果：网络 %s，广播 %s，可分配 %llu 个\n", net, bcast,
            (unsigned long long)ip_usable(prefix));
}

static const char *scope_result(IpKind kind, IpScope scope) {
    if (kind == IP_KIND_D) {
        return "D 类，组播";
    }
    if (kind == IP_KIND_E) {
        return "E 类，保留";
    }
    if (kind == IP_KIND_LOOP) {
        return "环回";
    }
    if (kind == IP_KIND_THIS) {
        return "本网络";
    }
    if (kind == IP_KIND_LIMITED) {
        return "受限广播";
    }
    if (scope == IP_SCOPE_PRIVATE) {
        return "私有地址";
    }
    if (scope == IP_SCOPE_SPECIAL) {
        return "链路本地";
    }
    return "公有地址";
}

void show_class(FILE *fp, uint32_t addr) {
    char ip[16];
    IpKind kind;
    IpScope scope;
    unsigned first = (addr >> 24) & 255u;

    ip_fmt(addr, ip, (int)sizeof ip);
    ip_classify(addr, &kind, &scope);
    fprintf(fp, "【分类与私有地址】\n");
    fprintf(fp, "地址 %s，第一字节 %u。\n", ip, first);
    fprintf(fp, "A 类 1～126，B 类 128～191，C 类 192～223，D 类 224～239，E 类 240～255。\n");
    fprintf(fp, "0 和 127 不拿来分配。私有地址是 10.0.0.0/8、172.16.0.0/12、192.168.0.0/16。\n");
    fprintf(fp, "%s", ip_kind_name(kind));
    if (kind == IP_KIND_A || kind == IP_KIND_B || kind == IP_KIND_C) {
        int pref = kind == IP_KIND_A ? 8 : kind == IP_KIND_B ? 16 : 24;
        fprintf(fp, "，默认前缀 /%d", pref);
    }
    fprintf(fp, "。%s。\n", scope_result(kind, scope));
    if (kind == IP_KIND_A || kind == IP_KIND_B || kind == IP_KIND_C) {
        fprintf(fp, "结果：%s，%s\n", ip_kind_name(kind), scope_result(kind, scope));
    } else {
        fprintf(fp, "结果：%s\n", scope_result(kind, scope));
    }
}

void show_equal(FILE *fp, Block base, unsigned hosts, const EqualSplit *s) {
    char text[32];
    int i;

    block_fmt(base, text, (int)sizeof text);
    fprintf(fp, "【定长子网】\n");
    fprintf(fp, "%s 划成 %d 个一样大的子网，每个至少 %u 个可分配地址。\n", text, s->count, hosts);
    fprintf(fp, "CIDR 的子网号可以全 0、全 1。主机号仍然不能全 0 或全 1。\n");
    fprintf(fp, "%d 个子网需要 %d 位子网号。\n", s->count, s->subnet_bits);
    fprintf(fp, "主机号 = 32 - %d - %d = %d 位，可分配 %llu 个。\n", base.prefix, s->subnet_bits, s->host_bits,
            (unsigned long long)s->usable);
    if (s->host_bits >= 2) {
        fprintf(fp, "其中 2^%d - 2 = %llu，去掉了网络地址和广播地址。\n", s->host_bits,
                (unsigned long long)s->usable);
    }
    fprintf(fp, "%llu ≥ %u，主机数够。\n", (unsigned long long)s->usable, hosts);
    for (i = 0; i < s->count; i++) {
        char mask[16];
        ip_fmt(prefix_mask(s->item[i].prefix), mask, (int)sizeof mask);
        fprintf(fp, "子网 %d：", i + 1);
        print_range(fp, s->item[i]);
        fprintf(fp, "，掩码 %s\n", mask);
    }
    fprintf(fp, "结果：%d 个子网，前缀 /%d，每个可分配 %llu 个\n", s->count, s->item[0].prefix,
            (unsigned long long)s->usable);
}

static void print_host_formula(FILE *fp, int prefix, uint64_t usable) {
    int host_bits = 32 - prefix;

    if (host_bits >= 2) {
        fprintf(fp, "主机号 %d 位，每个子网可分配 2^%d - 2 = %llu 个\n", host_bits, host_bits,
                (unsigned long long)usable);
        return;
    }
    fprintf(fp, "主机号 %d 位，每个子网可分配 %llu 个\n", host_bits, (unsigned long long)usable);
}

void show_mask(FILE *fp, Block base, const MaskSplit *s) {
    char text[32];
    char mask[16];

    block_fmt(base, text, (int)sizeof text);
    ip_fmt(prefix_mask(s->new_prefix), mask, (int)sizeof mask);
    fprintf(fp, "【定长子网】\n");
    fprintf(fp, "%s，子网掩码 %s，前缀 /%d\n", text, mask, s->new_prefix);
    fprintf(fp, "子网号 %d 位，子网个数 2^%d = %llu。\n", s->subnet_bits, s->subnet_bits,
            (unsigned long long)s->count);
    fprintf(fp, "CIDR 里全 0、全 1 的子网号都算，不按旧的 RFC 950 扣掉。\n");
    print_host_formula(fp, s->new_prefix, s->usable);
    fprintf(fp, "结果：%llu 个子网，每个可分配 %llu 个\n", (unsigned long long)s->count,
            (unsigned long long)s->usable);
}

void show_vlsm(FILE *fp, Block base, const VlsmSplit *s) {
    char text[32];
    uint64_t smallest = UINT64_MAX;
    int i;

    block_fmt(base, text, (int)sizeof text);
    fprintf(fp, "【变长子网】\n");
    fprintf(fp, "%s 划成 %d 块，并且盖住整个网络。\n", text, s->count);
    fprintf(fp, "要让最小的一块尽量小，就每次把剩下的一半划出去，最后两块一样大。\n");
    for (i = 0; i < s->count; i++) {
        uint64_t n = ip_usable(s->item[i].prefix);
        if (n < smallest) {
            smallest = n;
        }
        fprintf(fp, "块 %d：", i + 1);
        print_range(fp, s->item[i]);
        fputc('\n', fp);
    }
    fprintf(fp, "结果：最小子网可分配 %llu 个\n", (unsigned long long)smallest);
}

static void print_agg_byte(FILE *fp, const Block *in, int n, int prefix) {
    int index = prefix / 8;
    int same = prefix % 8;
    int i;

    if (prefix >= 32) {
        fprintf(fp, "这几块的 32 位完全相同。\n");
        return;
    }
    fprintf(fp, "看第 %d 字节：\n", index + 1);
    for (i = 0; i < n; i++) {
        char ip[32];
        char bits[9];
        block_fmt(in[i], ip, (int)sizeof ip);
        bits8(octet_of(in[i].base, index), bits);
        fprintf(fp, "%s  %s\n", ip, bits);
    }
    if (same == 0) {
        fprintf(fp, "前 %d 位相同，这一字节已经是主机号。\n", prefix);
    } else {
        fprintf(fp, "这一字节里相同的是前 %d 位，加上前面的 %d 位，前缀 /%d。\n", same, prefix - same, prefix);
    }
}

void show_agg(FILE *fp, const Block *in, int n, const AggResult *a, int has_via, uint32_t via) {
    char text[32];
    char mask[16];
    char via_ip[16];

    block_fmt(a->out, text, (int)sizeof text);
    ip_fmt(prefix_mask(a->out.prefix), mask, (int)sizeof mask);
    fprintf(fp, "【路由聚合】\n");
    fprintf(fp, "聚合取能盖住全部块的最长前缀，主机位清 0。\n");
    print_agg_byte(fp, in, n, a->out.prefix);
    fprintf(fp, "聚合后 %s，掩码 %s。\n", text, mask);
    if (a->exact) {
        fprintf(fp, "这几块刚好填满，没有多盖别的地址。\n");
    } else {
        fprintf(fp, "没有刚好填满，聚合块里还有没列出来的地址。\n");
    }
    fprintf(fp, "结果：%s", text);
    if (has_via) {
        ip_fmt(via, via_ip, (int)sizeof via_ip);
        fprintf(fp, "，掩码 %s，下一跳 %s", mask, via_ip);
    }
    fputc('\n', fp);
}

static void print_blocks(FILE *fp, const Block *b, int n) {
    int i;

    for (i = 0; i < n; i++) {
        char text[32];
        if (i > 0) {
            fputs("、", fp);
        }
        block_fmt(b[i], text, (int)sizeof text);
        fputs(text, fp);
    }
}

void show_sibling(FILE *fp, Block known, Block cand, int target, const SiblingResult *r) {
    char a[32];
    char c[32];
    char small[32];

    block_fmt(known, a, (int)sizeof a);
    block_fmt(cand, c, (int)sizeof c);
    fprintf(fp, "【子网能否凑齐】\n");
    fprintf(fp, "已知 %s，候选 %s，一共 %d 个子网。\n", a, c, target);
    if (r->overlap) {
        fprintf(fp, "两块地址重叠，不能同时当子网。\n");
        fprintf(fp, "结果：不可能\n");
        return;
    }
    block_fmt(r->small, small, (int)sizeof small);
    fprintf(fp, "两块不重叠。盖住它们的最小前缀是 %s。\n", small);
    if (r->n_holes == 0) {
        fprintf(fp, "里面没有空洞，目前是 %d 块。\n", r->at_small);
    } else {
        fprintf(fp, "还空着 ");
        print_blocks(fp, r->holes, r->n_holes);
        fprintf(fp, "，目前是 %d 块。\n", r->at_small);
    }
    if (!r->possible) {
        fprintf(fp, "%d > %d。往更大的网络扩只会再添兄弟块，块数不会变少。\n", r->at_small, target);
        fprintf(fp, "结果：不可能\n");
        return;
    }
    if (r->n_added > 0) {
        fprintf(fp, "再拼上兄弟块 ");
        print_blocks(fp, r->added, r->n_added);
        fprintf(fp, "，凑成 %d 块。\n", target);
    }
    fprintf(fp, "结果：可能");
    if (r->n_holes + r->n_added == 1) {
        fprintf(fp, "，另一块 ");
        print_blocks(fp, r->n_holes ? r->holes : r->added, 1);
    }
    fputc('\n', fp);
}

void show_remain(FILE *fp, Block net, uint32_t from, uint32_t to, uint32_t router, unsigned used,
                 uint64_t left) {
    char text[32];
    char a[16];
    char b[16];
    char gw[16];
    char first[16];
    char last[16];

    block_fmt(net, text, (int)sizeof text);
    ip_fmt(from, a, (int)sizeof a);
    ip_fmt(to, b, (int)sizeof b);
    ip_fmt(router, gw, (int)sizeof gw);
    ip_fmt(net.base + 1u, first, (int)sizeof first);
    ip_fmt(block_last(net) - 1u, last, (int)sizeof last);
    fprintf(fp, "【还能接多少主机】\n");
    fprintf(fp, "%s 可分配 %llu 个（%s～%s）。\n", text, (unsigned long long)ip_usable(net.prefix), first, last);
    fprintf(fp, "%s～%s 共 %u 个，路由器接口 %s 再占 1 个。\n", a, b, used, gw);
    fprintf(fp, "剩下 %llu - %u - 1 = %llu 个。\n", (unsigned long long)ip_usable(net.prefix), used,
            (unsigned long long)left);
    fprintf(fp, "结果：还可以连接 %llu 台\n", (unsigned long long)left);
}

void show_frag(FILE *fp, int total, int header, int mtu, int df, unsigned id, int has_id, const FragPlan *plan) {
    int i;

    fprintf(fp, "【IP 分片】\n");
    fprintf(fp, "总长度 %d 字节，首部 %d 字节，数据 %d 字节，MTU %d 字节。\n", total, header, plan->data_len,
            mtu);
    fprintf(fp, "除最后一片外，数据长度必须是 8 字节的倍数。片偏移的单位是 8 字节。\n");
    if (!plan->need) {
        fprintf(fp, "数据 %d 字节没有超过 MTU 减首部，不用分片。\n", plan->data_len);
        fprintf(fp, "结果：不分片，总长度 %d 字节，MF = 0，片偏移 0\n", total);
        return;
    }
    fprintf(fp, "单片最大数据 = ⌊(%d - %d) / 8⌋ × 8 = %d 字节\n", mtu, header, plan->max_data);
    if (plan->df_drop) {
        fprintf(fp, "DF = 1，路由器不能分片，只能丢弃。\n");
        fprintf(fp, "结果：DF = 1，不能分片\n");
        return;
    }
    fprintf(fp, "分片数 = ⌈%d / %d⌉ = %d\n", plan->data_len, plan->max_data, plan->count);
    for (i = 0; i < plan->count; i++) {
        const FragPiece *p = &plan->piece[i];
        fprintf(fp, "片 %d：数据 %d 字节，总长度 %d 字节，片偏移 %d，MF = %d", i + 1, p->data_len,
                p->total_len, p->offset_units, p->mf);
        if (has_id) {
            fprintf(fp, "，标识 %u", id);
        }
        if (df) {
            fprintf(fp, "，DF = 0");
        }
        fputc('\n', fp);
    }
    fprintf(fp, "结果：最大片数据 %d 字节，分片数 %d", plan->max_data, plan->count);
    if (plan->count >= 2) {
        fprintf(fp, "，第 2 片总长度 %d 字节，MF = %d", plan->piece[1].total_len, plan->piece[1].mf);
    }
    fprintf(fp, "，片偏移");
    for (i = 0; i < plan->count; i++) {
        fprintf(fp, " %d", plan->piece[i].offset_units);
    }
    fputc('\n', fp);
}
