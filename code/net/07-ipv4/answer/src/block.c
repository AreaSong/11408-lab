#include "block.h"

#include "scan.h"

#include <stdio.h>
#include <string.h>

static int need_aligned(Block b, char *err, int err_cap) {
    char bad[32];
    char good[32];
    char msg[128];
    Block net;

    if (b.prefix < 0 || b.prefix > 32) {
        return fail_msg(err, err_cap, "前缀要在 0 到 32");
    }
    if (block_aligned(b)) {
        return 1;
    }
    net.base = ip_network(b.base, b.prefix);
    net.prefix = b.prefix;
    block_fmt(b, bad, (int)sizeof bad);
    block_fmt(net, good, (int)sizeof good);
    snprintf(msg, sizeof msg, "%s 的主机位不是 0，要写成 %s", bad, good);
    return fail_msg(err, err_cap, msg);
}

static int ranges_overlap(Block a, Block b) {
    uint32_t a1 = block_last(a);
    uint32_t b1 = block_last(b);

    return a.base <= b1 && b.base <= a1;
}

int subnet_equal(Block base, unsigned hosts, int count, EqualSplit *out, char *err, int err_cap) {
    int bits = 0;
    int probe = count;
    int prefix;
    uint64_t step;
    int i;

    if (!need_aligned(base, err, err_cap)) {
        return 0;
    }
    if (count < 1 || (count & (count - 1)) != 0 || count > BLOCK_LIST) {
        return fail_msg(err, err_cap, "定长子网的个数要是 2 的幂，并且不超过 8");
    }
    while (probe > 1) {
        probe >>= 1;
        bits++;
    }
    if (base.prefix + bits > 32) {
        return fail_msg(err, err_cap, "主机位不够再划这些子网");
    }
    prefix = base.prefix + bits;
    out->count = count;
    out->subnet_bits = bits;
    out->host_bits = 32 - prefix;
    out->usable = ip_usable(prefix);
    if (out->usable < hosts) {
        return fail_msg(err, err_cap, "每个子网的可分配地址不够");
    }
    step = ip_space(prefix);
    for (i = 0; i < count; i++) {
        out->item[i].prefix = prefix;
        out->item[i].base = base.base + (uint32_t)(step * (uint64_t)i);
    }
    return 1;
}

int subnet_by_mask(Block base, int new_prefix, MaskSplit *out, char *err, int err_cap) {
    if (!need_aligned(base, err, err_cap)) {
        return 0;
    }
    if (new_prefix < base.prefix || new_prefix > 32) {
        return fail_msg(err, err_cap, "新掩码要比原网络更长，更短是聚合");
    }
    out->new_prefix = new_prefix;
    out->subnet_bits = new_prefix - base.prefix;
    out->count = 1ull << out->subnet_bits;
    out->usable = ip_usable(new_prefix);
    return 1;
}

int split_min(Block base, int parts, VlsmSplit *out, char *err, int err_cap) {
    int remain_prefix;
    uint32_t cursor;
    int i;

    if (!need_aligned(base, err, err_cap)) {
        return 0;
    }
    if (parts < 2 || parts > BLOCK_LIST) {
        return fail_msg(err, err_cap, "变长划分的块数要在 2 到 8");
    }
    if (base.prefix + parts - 1 > 32) {
        return fail_msg(err, err_cap, "地址不够切成这么多块");
    }
    cursor = base.base;
    remain_prefix = base.prefix;
    for (i = 0; i < parts - 1; i++) {
        out->item[i].prefix = remain_prefix + 1;
        out->item[i].base = cursor;
        cursor += (uint32_t)ip_space(out->item[i].prefix);
        remain_prefix++;
    }
    out->item[parts - 1].prefix = remain_prefix;
    out->item[parts - 1].base = cursor;
    out->count = parts;
    return 1;
}

static void sort_blocks(Block *b, int n) {
    int i;
    int j;

    for (i = 1; i < n; i++) {
        Block key = b[i];
        j = i;
        while (j > 0 && b[j - 1].base > key.base) {
            b[j] = b[j - 1];
            j--;
        }
        b[j] = key;
    }
}

static int cover_of(const Block *b, int n, Block *out) {
    uint32_t lo = b[0].base;
    uint32_t hi = block_last(b[0]);
    uint32_t diff;
    int prefix = 32;
    int i;

    for (i = 1; i < n; i++) {
        uint32_t last = block_last(b[i]);
        if (b[i].base < lo) {
            lo = b[i].base;
        }
        if (last > hi) {
            hi = last;
        }
    }
    diff = lo ^ hi;
    while (diff != 0) {
        diff >>= 1;
        prefix--;
    }
    out->prefix = prefix;
    out->base = ip_network(lo, prefix);
    return 1;
}

static int chunk_bits(uint32_t start, uint32_t end) {
    uint64_t len = (uint64_t)end - start + 1ull;
    int fit = 0;
    int align = 32;
    uint32_t x = start;

    while (fit < 32 && (1ull << (fit + 1)) <= len) {
        fit++;
    }
    if (start != 0) {
        align = 0;
        while ((x & 1u) == 0) {
            x >>= 1;
            align++;
        }
    }
    return fit < align ? fit : align;
}

static int emit_gap(uint32_t start, uint32_t end, Block *out, int cap, int *n) {
    while (1) {
        int bits = chunk_bits(start, end);
        uint64_t next;

        if (*n >= cap) {
            return 0;
        }
        out[*n].base = start;
        out[*n].prefix = 32 - bits;
        (*n)++;
        if (bits >= 32) {
            break;
        }
        next = (uint64_t)start + (1ull << bits);
        if (next > (uint64_t)end) {
            break;
        }
        start = (uint32_t)next;
    }
    return 1;
}

static int holes_between(Block parent, Block *sorted, int n, Block *out, int cap, int *nout) {
    uint32_t cursor = parent.base;
    uint32_t parent_last = block_last(parent);
    int i;

    *nout = 0;
    for (i = 0; i < n; i++) {
        uint32_t last = block_last(sorted[i]);
        if (sorted[i].base > cursor && !emit_gap(cursor, sorted[i].base - 1u, out, cap, nout)) {
            return 0;
        }
        if (last == 0xffffffffu) {
            return 1;
        }
        cursor = last + 1u;
    }
    if (cursor <= parent_last && !emit_gap(cursor, parent_last, out, cap, nout)) {
        return 0;
    }
    return 1;
}

int aggregate_blocks(const Block *in, int n, AggResult *out, char *err, int err_cap) {
    Block copy[BLOCK_LIST];
    uint64_t sum = 0;
    int i;
    int j;

    if (n < 2 || n > BLOCK_LIST) {
        return fail_msg(err, err_cap, "聚合至少 2 块，最多 8 块");
    }
    for (i = 0; i < n; i++) {
        if (!need_aligned(in[i], err, err_cap)) {
            return 0;
        }
        for (j = 0; j < i; j++) {
            if (ranges_overlap(in[i], in[j])) {
                return fail_msg(err, err_cap, "这些块有重叠");
            }
        }
        copy[i] = in[i];
        sum += ip_space(in[i].prefix);
    }
    cover_of(copy, n, &out->out);
    out->exact = sum == ip_space(out->out.prefix);
    return 1;
}

static int grow_sibling(Block *current, Block *sib) {
    Block parent;

    if (current->prefix <= 0) {
        return 0;
    }
    parent.prefix = current->prefix - 1;
    parent.base = current->base & prefix_mask(parent.prefix);
    sib->prefix = current->prefix;
    if (parent.base == current->base) {
        sib->base = current->base + (uint32_t)ip_space(current->prefix);
    } else {
        sib->base = parent.base;
    }
    *current = parent;
    return 1;
}

int sibling_blocks(Block known, Block cand, int target, SiblingResult *out, char *err, int err_cap) {
    Block pair[2];
    Block cur;
    int need;
    int k;

    memset(out, 0, sizeof *out);
    if (!need_aligned(known, err, err_cap) || !need_aligned(cand, err, err_cap)) {
        return 0;
    }
    if (target < 2 || target > BLOCK_LIST) {
        return fail_msg(err, err_cap, "子网个数要在 2 到 8");
    }
    if (ranges_overlap(known, cand)) {
        out->overlap = 1;
        return 1;
    }
    pair[0] = known;
    pair[1] = cand;
    cover_of(pair, 2, &out->small);
    sort_blocks(pair, 2);
    if (!holes_between(out->small, pair, 2, out->holes, BLOCK_LIST, &out->n_holes)) {
        return fail_msg(err, err_cap, "空洞太碎，列不下");
    }
    out->at_small = 2 + out->n_holes;
    /* 2021-35：最小覆盖里若已经超过题目的块数，再往上扩只会多一个兄弟块，凑不回去。 */
    if (out->at_small > target) {
        return 1;
    }
    cur = out->small;
    need = target - out->at_small;
    for (k = 0; k < need; k++) {
        if (!grow_sibling(&cur, &out->added[k])) {
            return fail_msg(err, err_cap, "扩到整段地址也不够这么多块");
        }
    }
    out->n_added = need;
    out->possible = 1;
    return 1;
}

static int assignable(Block net, uint32_t ip) {
    if (net.prefix >= 32) {
        return ip == net.base;
    }
    if (net.prefix >= 31) {
        return 0;
    }
    return ip > net.base && ip < block_last(net);
}

int hosts_left(Block net, uint32_t from, uint32_t to, uint32_t router, unsigned *used, uint64_t *left,
               char *err, int err_cap) {
    unsigned n;
    uint64_t usable;

    if (!need_aligned(net, err, err_cap)) {
        return 0;
    }
    if (net.prefix >= 31) {
        return fail_msg(err, err_cap, "这个前缀没有成段的可分配主机");
    }
    if (from > to || !assignable(net, from) || !assignable(net, to)) {
        return fail_msg(err, err_cap, "主机段要落在这个子网的可分配地址里");
    }
    if (!assignable(net, router)) {
        return fail_msg(err, err_cap, "路由器接口不在可分配地址里");
    }
    if (router >= from && router <= to) {
        return fail_msg(err, err_cap, "路由器地址已经算进主机段里了");
    }
    n = to - from + 1u;
    usable = ip_usable(net.prefix);
    if ((uint64_t)n + 1ull > usable) {
        return fail_msg(err, err_cap, "分配超出了这个子网");
    }
    *used = n;
    *left = usable - n - 1ull;
    return 1;
}
