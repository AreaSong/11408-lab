#include "inode.h"

#include <stdio.h>
#include <string.h>

static int mul(uint64_t a, uint64_t b, uint64_t *out) {
    if (a != 0 && b > UINT64_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static int add(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static int spec_ok(const InodeSpec *spec) {
    if (spec == 0 || spec->block_size == 0 || spec->addr_size == 0) {
        return 0;
    }
    if (spec->block_size % spec->addr_size != 0) {
        return 0;
    }
    if (spec->direct < 0 || spec->single < 0 || spec->doubly < 0 || spec->triply < 0) {
        return 0;
    }
    return 1;
}

int inode_max(const InodeSpec *spec, InodeMax *out) {
    uint64_t ptrs;
    uint64_t sq;
    uint64_t cu;
    uint64_t blocks = 0;
    int i;
    int counts[4];

    if (!spec_ok(spec) || out == 0) {
        return -1;
    }
    ptrs = spec->block_size / spec->addr_size;
    if (mul(ptrs, ptrs, &sq) != 0 || mul(sq, ptrs, &cu) != 0) {
        return -1;
    }
    counts[0] = spec->direct;
    counts[1] = spec->single;
    counts[2] = spec->doubly;
    counts[3] = spec->triply;
    memset(out, 0, sizeof *out);
    out->ptrs = ptrs;
    for (i = 0; i < 4; i++) {
        uint64_t span = (uint64_t)counts[i];
        uint64_t factor = i == 0 ? 1 : i == 1 ? ptrs : i == 2 ? sq : cu;
        if (mul(span, factor, &out->level_blocks[i]) != 0) {
            return -1;
        }
        if (mul(out->level_blocks[i], spec->block_size, &out->level_bytes[i]) != 0) {
            return -1;
        }
        if (add(blocks, out->level_blocks[i], &blocks) != 0) {
            return -1;
        }
    }
    if (mul(blocks, spec->block_size, &out->bytes) != 0) {
        return -1;
    }
    out->blocks = blocks;
    return 0;
}

static int append_unit(char *buf, int cap, int *at, uint64_t bytes) {
    static const uint64_t unit_b[] = {1ull << 40, 1ull << 30, 1ull << 20, 1ull << 10, 1};
    static const char *unit_s[] = {"TB", "GB", "MB", "KB", "B"};
    uint64_t n = bytes;
    const char *unit = "B";
    int i;
    int wrote;

    if (bytes == 0) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        if (bytes % unit_b[i] == 0) {
            n = bytes / unit_b[i];
            unit = unit_s[i];
            break;
        }
    }
    if (*at > 0) {
        if (*at + 1 >= cap) {
            return -1;
        }
        buf[(*at)++] = '+';
        buf[*at] = '\0';
    }
    wrote = snprintf(buf + *at, (size_t)(cap - *at), "%llu%s", (unsigned long long)n, unit);
    if (wrote < 0 || *at + wrote >= cap) {
        return -1;
    }
    *at += wrote;
    return 0;
}

int inode_expr(const InodeMax *max, char *buf, int cap) {
    int at = 0;
    int i;
    int any = 0;

    if (max == 0 || buf == 0 || cap < 2) {
        return -1;
    }
    buf[0] = '\0';
    for (i = 0; i < 4; i++) {
        if (append_unit(buf, cap, &at, max->level_bytes[i]) != 0) {
            return -1;
        }
        if (max->level_bytes[i] != 0) {
            any = 1;
        }
    }
    if (!any) {
        snprintf(buf, (size_t)cap, "0");
    }
    return 0;
}

const char *inode_level_name(int level) {
    switch (level) {
    case 0:
        return "direct";
    case 1:
        return "single";
    case 2:
        return "double";
    case 3:
        return "triple";
    default:
        return "empty";
    }
}

int inode_hit(const InodeSpec *spec, uint64_t offset, InodeHit *hit) {
    InodeMax max;
    uint64_t logical;
    uint64_t base = 0;
    int level;

    if (hit == 0 || inode_max(spec, &max) != 0) {
        return -1;
    }
    if (offset >= max.bytes) {
        return -2;
    }
    logical = offset / spec->block_size;
    for (level = 0; level < 4; level++) {
        uint64_t span = max.level_blocks[level];
        if (logical < base + span) {
            hit->level = level;
            hit->logical = logical;
            /* 索引结点已在内存。直接地址不再读索引块，第 level 级要读 level 个索引块。 */
            hit->addr_reads = level;
            hit->data_reads = level + 1;
            return 0;
        }
        base += span;
    }
    return -2;
}

static void indirect_text(int level, char *buf, int cap) {
    if (level <= 0) {
        snprintf(buf, (size_t)cap, "none");
        return;
    }
    if (level == 1) {
        snprintf(buf, (size_t)cap, "1");
        return;
    }
    if (level == 2) {
        snprintf(buf, (size_t)cap, "1+2");
        return;
    }
    snprintf(buf, (size_t)cap, "1+2+3");
}

int inode_cover(const InodeSpec *spec, uint64_t file_bytes, InodeCover *cover) {
    InodeMax max;
    InodeHit hit;
    uint64_t blocks;

    if (cover == 0 || inode_max(spec, &max) != 0) {
        return -1;
    }
    memset(cover, 0, sizeof *cover);
    if (file_bytes == 0) {
        indirect_text(-1, cover->indirect, (int)sizeof cover->indirect);
        cover->level = -1;
        return 0;
    }
    if (file_bytes > max.bytes) {
        return -2;
    }
    blocks = (file_bytes + spec->block_size - 1) / spec->block_size;
    if (inode_hit(spec, (blocks - 1) * spec->block_size, &hit) != 0) {
        return -1;
    }
    cover->blocks = blocks;
    cover->level = hit.level;
    cover->data_reads = hit.data_reads;
    cover->addr_reads = hit.addr_reads;
    indirect_text(hit.level, cover->indirect, (int)sizeof cover->indirect);
    return 0;
}
