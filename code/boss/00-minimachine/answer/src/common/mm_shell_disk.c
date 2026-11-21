#include "fs/fs_free.h"
#include "fs/fs_ftl.h"
#include "fs/fs_geom.h"
#include "fs/fs_index.h"
#include "fs/fs_raid.h"
#include "fs/fs_sched.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static const char *skip_words(const char *s, int n) {
    while (n > 0) {
        while (*s == ' ' || *s == '\t') {
            s++;
        }
        if (*s == '\0') {
            return s;
        }
        while (*s != '\0' && *s != ' ' && *s != '\t') {
            s++;
        }
        n--;
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static int read_ints(const char *s, int *out, int cap, int *n) {
    *n = 0;
    while (*s != '\0') {
        char *end = NULL;
        long value = strtol(s, &end, 10);
        if (end == s) {
            return -1;
        }
        if (*n >= cap) {
            return -1;
        }
        out[(*n)++] = (int)value;
        s = end;
    }
    return 0;
}

static int cmd_arm(const char *arg, FILE *out) {
    char algo[16];
    char dir[16];
    char maxs[16];
    int head = 0;
    int req[FS_SCHED_N];
    int n = 0;
    int way = 0;
    int has = 0;
    int max_track = 0;
    FsSched got;
    if (sscanf(arg, "%15s %d %15s %15s", algo, &head, dir, maxs) != 4) {
        fprintf(out, "错误：arm 算法 磁头 up|down|none 最大磁道|none 请求…\n");
        return -1;
    }
    if (strcmp(dir, "up") == 0) {
        way = 1;
    } else if (strcmp(dir, "down") == 0) {
        way = -1;
    } else if (strcmp(dir, "none") != 0) {
        fprintf(out, "错误：方向只能是 up、down 或 none\n");
        return -1;
    }
    if (strcmp(maxs, "none") != 0) {
        has = 1;
        max_track = atoi(maxs);
    }
    if (read_ints(skip_words(arg, 4), req, FS_SCHED_N, &n) != 0 ||
        fs_sched(algo, head, way, has, max_track, req, n, &got) != 0) {
        fprintf(out, "错误：这条调度算不了\n");
        return -1;
    }
    fs_sched_print(out, algo, &got);
    return 1;
}

static int cmd_time(const char *arg, FILE *out) {
    char kind[16];
    long long a = 0;
    long long b = 0;
    long long c = 0;
    long long d = 0;
    long long e = 0;
    FsTime part;
    int rc = -1;
    if (sscanf(arg, "%15s %lld %lld %lld %lld %lld", kind, &a, &b, &c, &d, &e) != 6) {
        fprintf(out, "错误：time each|rate|sum 后面要 5 个数\n");
        return -1;
    }
    if (strcmp(kind, "each") == 0) {
        rc = fs_time_each(a, (int)b, (int)c, (int)d, e, &part);
    } else if (strcmp(kind, "rate") == 0) {
        rc = fs_time_rate(a, (int)b, c, d, e, &part);
    } else if (strcmp(kind, "sum") == 0) {
        rc = fs_time_sum(a, (int)b, (int)c, (int)d, e, &part);
    }
    if (rc != 0) {
        fprintf(out, "错误：时间算不了\n");
        return -1;
    }
    fs_time_print(out, &part);
    return 1;
}

static int cmd_chs(const char *arg, FILE *out) {
    char kind[16];
    int a, b, c, d;
    int order[FS_SCHED_N];
    int req[FS_SCHED_N];
    int n = 0;
    int i;
    if (sscanf(arg, "%15s", kind) != 1) {
        fprintf(out, "错误：chs bytes|at|sstf\n");
        return -1;
    }
    if (strcmp(kind, "bytes") == 0) {
        if (sscanf(rest(arg, "bytes"), "%d %d %d %d", &a, &b, &c, &d) != 4) {
            fprintf(out, "错误：chs bytes 柱面 磁头 扇区 字节\n");
            return -1;
        }
        fprintf(out, "bytes=%lld\n", fs_disk_bytes(a, b, c, d));
        return 1;
    }
    if (strcmp(kind, "at") == 0) {
        int cyl, head, sector;
        if (sscanf(rest(arg, "at"), "%d %d %d %d", &a, &b, &c, &d) != 4 ||
            fs_cluster_chs(a, b, c, d, &cyl, &head, &sector) != 0) {
            fprintf(out, "错误：chs at 算不了\n");
            return -1;
        }
        fprintf(out, "cyl=%d head=%d sector=%d\n", cyl, head, sector);
        return 1;
    }
    if (strcmp(kind, "sstf") != 0 || sscanf(rest(arg, "sstf"), "%d %d %d %d", &a, &b, &c, &d) != 4 ||
        read_ints(skip_words(rest(arg, "sstf"), 4), req, FS_SCHED_N, &n) != 0 ||
        fs_cluster_sstf(a, b, c, d, req, n, order) != 0) {
        fprintf(out, "错误：chs sstf 算不了\n");
        return -1;
    }
    fprintf(out, "order=");
    for (i = 0; i < n; i++) {
        fprintf(out, "%s%d", i ? " " : "", order[i]);
    }
    fprintf(out, "\n");
    return 1;
}

static int load_spec(const char *s, FsInodeSpec *spec) {
    unsigned long long block = 0;
    unsigned long long addr = 0;
    if (sscanf(s, "%llu %llu %d %d %d %d", &block, &addr, &spec->direct, &spec->single, &spec->doubly,
               &spec->triply) != 6) {
        return -1;
    }
    spec->block_size = block;
    spec->addr_size = addr;
    return 0;
}

static int cmd_inode(const char *arg, FILE *out) {
    char kind[16];
    FsInodeSpec spec;
    FsInodeMax max;
    FsInodeHit hit;
    FsInodeCover cover;
    char expr[64];
    unsigned long long off = 0;
    if (sscanf(arg, "%15s", kind) != 1 || load_spec(rest(arg, kind), &spec) != 0) {
        fprintf(out, "错误：inode max|read|need 块 地址项 直接 一级 二级 三级 [偏移]\n");
        return -1;
    }
    if (strcmp(kind, "max") == 0) {
        if (fs_inode_max(&spec, &max) != 0 || fs_inode_expr(&max, expr, (int)sizeof expr) != 0) {
            fprintf(out, "错误：最大长度算不了\n");
            return -1;
        }
        fprintf(out, "bytes=%llu blocks=%llu expr=%s\n", (unsigned long long)max.bytes,
                (unsigned long long)max.blocks, expr);
        return 1;
    }
    if (sscanf(skip_words(rest(arg, kind), 6), "%llu", &off) != 1) {
        fprintf(out, "错误：还要一个偏移或字节数\n");
        return -1;
    }
    if (strcmp(kind, "read") == 0 && fs_inode_hit(&spec, off, &hit) == 0) {
        fprintf(out, "level=%s logical=%llu data-reads=%d addr-reads=%d\n", fs_inode_level(hit.level),
                (unsigned long long)hit.logical, hit.data_reads, hit.addr_reads);
        return 1;
    }
    if (strcmp(kind, "need") == 0 && fs_inode_cover(&spec, off, &cover) == 0) {
        fprintf(out, "blocks=%llu level=%s indirect=%s data-reads=%d\n", (unsigned long long)cover.blocks,
                fs_inode_level(cover.level), cover.indirect, cover.data_reads);
        return 1;
    }
    fprintf(out, "错误：inode 算不了\n");
    return -1;
}

static FsGroup *group_slot(void) {
    static FsGroup group;
    return &group;
}

static int cmd_group_edit(const char *arg, const char *kind, FsGroup *g, FILE *out) {
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    int got = 0;
    int did = 0;
    int stack[FS_GROUP_CAP];
    int n = 0;
    if (strcmp(kind, "fill") == 0) {
        if (sscanf(rest(arg, "fill"), "%d %d %d %d", &a, &b, &c, &d) != 4 ||
            fs_group_fill(g, a, b, c, d) != 0) {
            fprintf(out, "错误：group fill 失败\n");
            return -1;
        }
        fprintf(out, "fill block=%d n=%d bottom=%d top=%d\n", a, b, c, c + (b - 1) * d);
        return 1;
    }
    if (strcmp(kind, "stack") == 0) {
        if (read_ints(rest(arg, "stack"), stack, FS_GROUP_CAP, &n) != 0 || fs_group_set_stack(g, stack, n) != 0 ||
            g->n <= 0) {
            return -1;
        }
        fprintf(out, "stack n=%d bottom=%d top=%d\n", g->n, g->blk[0], g->blk[g->n - 1]);
        return 1;
    }
    if (strcmp(kind, "total") == 0) {
        fprintf(out, "total n=%d\n", fs_group_total(g));
        return 1;
    }
    if (strcmp(kind, "alloc") == 0) {
        if (fs_group_alloc(g, &got, &did) != 0) {
            return -1;
        }
        fprintf(out, "alloc block=%d read=%d n=%d\n", got, did, g->n);
        return 1;
    }
    if (strcmp(kind, "free") == 0) {
        if (sscanf(rest(arg, "free"), "%d", &a) != 1 || fs_group_free(g, a, &did) != 0) {
            return -1;
        }
        fprintf(out, "free block=%d write=%d n=%d\n", a, did, g->n);
        return 1;
    }
    return 0;
}

static int cmd_group(const char *arg, FILE *out) {
    char kind[16];
    FsGroup *g = group_slot();
    int edited;
    int i;
    int a = 0;
    if (sscanf(arg, "%15s", kind) != 1) {
        fprintf(out, "错误：group cap|fill|stack|total|alloc|free|show\n");
        return -1;
    }
    if (strcmp(kind, "cap") == 0) {
        if (sscanf(rest(arg, "cap"), "%d", &a) != 1) {
            return -1;
        }
        fs_group_init(g, a);
        fprintf(out, "group cap=%d\n", g->cap);
        return 1;
    }
    edited = cmd_group_edit(arg, kind, g, out);
    if (edited != 0) {
        return edited;
    }
    if (g->n <= 0) {
        fprintf(out, "stack n=0\n");
        return 1;
    }
    fprintf(out, "stack n=%d bottom=%d top=%d\n", g->n, g->blk[0], g->blk[g->n - 1]);
    for (i = 0; i < g->n; i++) {
        fprintf(out, "blk %d\n", g->blk[i]);
    }
    return 1;
}

static int cmd_bitmap(const char *arg, FILE *out) {
    char kind[16];
    unsigned long long a = 0;
    unsigned long long b = 0;
    unsigned long long c = 0;
    unsigned long long d = 0;
    unsigned long long blocks = 0;
    unsigned long long map_bytes = 0;
    unsigned long long map_blocks = 0;
    if (sscanf(arg, "%15s %llu %llu %llu %llu", kind, &a, &b, &c, &d) < 3) {
        fprintf(out, "错误：bitmap size|pos|block\n");
        return -1;
    }
    if (strcmp(kind, "size") == 0 && fs_bitmap_size(a, b, &blocks, &map_bytes, &map_blocks) == 0) {
        fprintf(out, "blocks=%llu map-bytes=%llu map-blocks=%llu\n", (unsigned long long)blocks,
                (unsigned long long)map_bytes, (unsigned long long)map_blocks);
        return 1;
    }
    if (strcmp(kind, "pos") == 0 && fs_bitmap_locate(a, (int)b, (int)c, &blocks, &map_bytes) == 0) {
        fprintf(out, "word=%llu bit=%llu\n", (unsigned long long)blocks, (unsigned long long)map_bytes);
        return 1;
    }
    if (strcmp(kind, "block") == 0 && fs_bitmap_compose(a, b, (int)c, (int)d, &blocks) == 0) {
        fprintf(out, "block=%llu\n", (unsigned long long)blocks);
        return 1;
    }
    fprintf(out, "错误：位示图算不了\n");
    return -1;
}

static int cmd_wear(const char *arg, FILE *out) {
    int nblk, npg, times;
    char policy[16];
    int erase[FS_FTL_BLKS];
    int host = 0;
    int flash = 0;
    int dynamic;
    int i;
    if (sscanf(arg, "%d %d %d %15s", &nblk, &npg, &times, policy) != 4) {
        fprintf(out, "错误：wear 块数 页数 次数 none|dynamic\n");
        return -1;
    }
    dynamic = strcmp(policy, "dynamic") == 0;
    if (fs_ftl_wear(nblk, npg, times, dynamic, erase, &host, &flash) != 0) {
        fprintf(out, "错误：磨损均衡算不了\n");
        return -1;
    }
    fprintf(out, "policy=%s host=%d flash=%d erase=", dynamic ? "dynamic" : "none", host, flash);
    for (i = 0; i < nblk; i++) {
        fprintf(out, "%s%d", i ? "," : "", erase[i]);
    }
    fprintf(out, "\n");
    return 1;
}

static int cmd_raid(const char *arg, FILE *out) {
    int level, disks, stripes, s, d;
    FsRaid raid;
    if (sscanf(arg, "%d %d %d", &level, &disks, &stripes) != 3 ||
        fs_raid_plan(level, disks, stripes, &raid) != 0) {
        fprintf(out, "错误：raid 级别 盘数 条带数\n");
        return -1;
    }
    fprintf(out, "raid level=%d disks=%d stripes=%d usable=%d parity=%d tolerate=%d\n", raid.level,
            raid.disks, raid.stripes, raid.usable, raid.parity, raid.tolerate);
    for (s = 0; s < raid.stripes; s++) {
        fprintf(out, "row%d", s);
        for (d = 0; d < raid.disks; d++) {
            fprintf(out, " %s", raid.cell[s][d]);
        }
        fprintf(out, "\n");
    }
    return 1;
}

static int cmd_phys(const char *arg, FILE *out) {
    char kind[16];
    char name[16];
    int k = 0;
    int reads = 0;
    int next = 0;
    unsigned long long block = 0;
    unsigned long long ptr = 0;
    unsigned long long bytes = 0;
    FsInsert info;
    if (sscanf(arg, "%15s", kind) != 1) {
        return -1;
    }
    if (strcmp(kind, "visit") == 0) {
        if (sscanf(rest(arg, "visit"), "%15s %d", name, &k) != 2 ||
            fs_phys_visit(fs_phys_kind(name), k, &reads, &next) != 0) {
            fprintf(out, "错误：phys visit 类型 第几块\n");
            return -1;
        }
        fprintf(out, "visit %s k=%d reads=%d next=%d\n", name, k, reads, next);
        return 1;
    }
    if (strcmp(kind, "linkmax") == 0) {
        if (sscanf(rest(arg, "linkmax"), "%llu %llu", &block, &ptr) != 2 || fs_link_max(block, ptr, &bytes) != 0) {
            return -1;
        }
        fprintf(out, "linkmax bytes=%llu gb=%llu\n", (unsigned long long)bytes,
                (unsigned long long)(bytes / (1024ull * 1024ull * 1024ull)));
        return 1;
    }
    if (sscanf(arg, "%15s %15s %d %d", kind, name, &k, &reads) != 4) {
        fprintf(out, "错误：phys insert cont|link 条数 位置\n");
        return -1;
    }
    if (strcmp(name, "cont") == 0 && fs_insert_cont(k, reads, &info) == 0) {
        fprintf(out, "insert cont move=%d dir=%s reads=%d\n", info.move, info.head ? "head" : "tail", info.reads);
        return 1;
    }
    if (strcmp(name, "link") == 0 && fs_insert_link(k, reads, &next) == 0) {
        fprintf(out, "insert link reads=%d\n", next);
        return 1;
    }
    fprintf(out, "错误：物理结构算不了\n");
    return -1;
}

int mm_shell_disk(char *line, FILE *out) {
    if (starts(line, "arm")) {
        return cmd_arm(rest(line, "arm"), out);
    }
    if (starts(line, "time")) {
        return cmd_time(rest(line, "time"), out);
    }
    if (starts(line, "chs")) {
        return cmd_chs(rest(line, "chs"), out);
    }
    if (starts(line, "inode")) {
        return cmd_inode(rest(line, "inode"), out);
    }
    if (starts(line, "bitmap")) {
        return cmd_bitmap(rest(line, "bitmap"), out);
    }
    if (starts(line, "group")) {
        return cmd_group(rest(line, "group"), out);
    }
    if (starts(line, "wear")) {
        return cmd_wear(rest(line, "wear"), out);
    }
    if (starts(line, "raid")) {
        return cmd_raid(rest(line, "raid"), out);
    }
    if (starts(line, "phys")) {
        return cmd_phys(rest(line, "phys"), out);
    }
    return 0;
}
