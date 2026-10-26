#include "run.h"

#include "bitmap.h"
#include "exam.h"
#include "inode.h"
#include "phys.h"

#include <stdio.h>
#include <string.h>

static int soft(const char *msg) {
    printf("result=%s\n", msg);
    return 0;
}

static int load_spec(char tok[][TOK_W], int at, InodeSpec *spec) {
    unsigned long long bs;
    unsigned long long as;
    int direct;
    int single;
    int doubly;
    int triply;

    if (take_u64(tok[at], 1, 1048576, &bs) || take_u64(tok[at + 1], 1, 8, &as) ||
        take_i(tok[at + 2], 0, 64, &direct) || take_i(tok[at + 3], 0, 64, &single) ||
        take_i(tok[at + 4], 0, 64, &doubly) || take_i(tok[at + 5], 0, 64, &triply)) {
        return 1;
    }
    spec->block_size = bs;
    spec->addr_size = as;
    spec->direct = direct;
    spec->single = single;
    spec->doubly = doubly;
    spec->triply = triply;
    return 0;
}

static int cmd_phys(char tok[][TOK_W], int ntok) {
    int k;
    int kind;

    if (need(ntok, 3) || take_i(tok[2], 1, 1000000, &k)) {
        return 1;
    }
    if (strcmp(tok[1], "all") == 0) {
        for (kind = PHYS_CONT; kind <= PHYS_INDEX; kind++) {
            int reads = 0;
            int next = 0;
            if (kind == PHYS_FAT_DISK) {
                continue;
            }
            phys_visit(kind, k, &reads, &next);
            printf("phys kind=%s k=%d reads=%d next=%d\n", phys_name(kind), k, reads, next);
        }
        return 0;
    }
    kind = phys_kind(tok[1]);
    if (kind < 0) {
        return fail("bad-command");
    }
    {
        int reads = 0;
        int next = 0;
        phys_visit(kind, k, &reads, &next);
        printf("phys kind=%s k=%d reads=%d next=%d\n", phys_name(kind), k, reads, next);
    }
    return 0;
}

static int cmd_inode_max(char tok[][TOK_W], int ntok) {
    InodeSpec spec;
    InodeMax max;
    char expr[80];

    if (need(ntok, 8) || load_spec(tok, 2, &spec)) {
        return 1;
    }
    if (inode_max(&spec, &max) != 0 || inode_expr(&max, expr, (int)sizeof expr) != 0) {
        return fail("bad-number");
    }
    printf("max blocks=%llu bytes=%llu expr=%s\n", (unsigned long long)max.blocks,
           (unsigned long long)max.bytes, expr);
    return 0;
}

static int cmd_inode_read(char tok[][TOK_W], int ntok) {
    InodeSpec spec;
    InodeHit hit;
    unsigned long long off;
    int rc;

    if (need(ntok, 9) || load_spec(tok, 2, &spec) || take_u64(tok[8], 0, ~0ull, &off)) {
        return 1;
    }
    rc = inode_hit(&spec, off, &hit);
    if (rc == -2) {
        return soft("range");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    printf("read off=%llu logical=%llu nth=%llu level=%s data-reads=%d addr-reads=%d\n",
           off, (unsigned long long)hit.logical, (unsigned long long)hit.logical + 1,
           inode_level_name(hit.level), hit.data_reads, hit.addr_reads);
    return 0;
}

static int cmd_inode_need(char tok[][TOK_W], int ntok) {
    InodeSpec spec;
    InodeCover cover;
    unsigned long long bytes;
    int rc;

    if (need(ntok, 9) || load_spec(tok, 2, &spec) || take_u64(tok[8], 0, ~0ull, &bytes)) {
        return 1;
    }
    rc = inode_cover(&spec, bytes, &cover);
    if (rc == -2) {
        return soft("range");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    printf("need bytes=%llu blocks=%llu level=%s indirect=%s\n", bytes,
           (unsigned long long)cover.blocks, inode_level_name(cover.level), cover.indirect);
    return 0;
}

static int cmd_inode(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "max") == 0) {
        return cmd_inode_max(tok, ntok);
    }
    if (strcmp(tok[1], "read") == 0) {
        return cmd_inode_read(tok, ntok);
    }
    if (strcmp(tok[1], "need") == 0) {
        return cmd_inode_need(tok, ntok);
    }
    return fail("bad-command");
}

static int bitmap_blocks_cmd(char tok[][TOK_W], int ntok) {
    unsigned long long disk;
    unsigned long long block;
    uint64_t blocks;
    uint64_t map_bytes;
    uint64_t map_blocks;

    if (need(ntok, 4) || take_u64(tok[2], 1, ~0ull, &disk) || take_u64(tok[3], 1, ~0ull, &block)) {
        return 1;
    }
    if (bitmap_size(disk, block, &blocks, &map_bytes, &map_blocks) != 0) {
        return fail("bad-number");
    }
    printf("bitmap blocks=%llu map-bytes=%llu map-blocks=%llu\n", (unsigned long long)blocks,
           (unsigned long long)map_bytes, (unsigned long long)map_blocks);
    return 0;
}

static int cmd_bitmap(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "blocks") == 0) {
        return bitmap_blocks_cmd(tok, ntok);
    }
    if (strcmp(tok[1], "pos") == 0) {
        unsigned long long block;
        int bits;
        int origin;
        uint64_t word;
        uint64_t bit;
        if (need(ntok, 5) || take_u64(tok[2], 0, ~0ull, &block) || take_i(tok[3], 1, 64, &bits) ||
            take_i(tok[4], 0, 1, &origin)) {
            return 1;
        }
        if (bitmap_locate(block, bits, origin, &word, &bit) != 0) {
            return fail("bad-number");
        }
        printf("bitmap block=%llu word=%llu bit=%llu\n", block, (unsigned long long)word,
               (unsigned long long)bit);
        return 0;
    }
    if (strcmp(tok[1], "block") == 0) {
        unsigned long long word;
        unsigned long long bit;
        int bits;
        int origin;
        uint64_t block;
        if (need(ntok, 6) || take_u64(tok[2], 0, ~0ull, &word) || take_u64(tok[3], 0, ~0ull, &bit) ||
            take_i(tok[4], 1, 64, &bits) || take_i(tok[5], 0, 1, &origin)) {
            return 1;
        }
        if (bitmap_compose(word, bit, bits, origin, &block) != 0) {
            return fail("bad-number");
        }
        printf("bitmap word=%llu bit=%llu block=%llu\n", word, bit, (unsigned long long)block);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_insert(char tok[][TOK_W], int ntok) {
    int n;
    int pos;

    if (need(ntok, 4) || take_i(tok[2], 1, 1000000, &n)) {
        return 1;
    }
    if (take_i(tok[3], 1, n, &pos)) {
        return 1;
    }
    if (strcmp(tok[1], "cont") == 0) {
        InsertInfo info;
        const char *fcb;
        if (insert_cont(n, pos, &info) != 0) {
            return fail("bad-number");
        }
        fcb = info.head ? "start+length" : "length";
        printf("insert cont n=%d pos=%d move=%d dir=%s reads=%d fcb=%s\n", n, pos, info.move,
               info.head ? "head" : "tail", info.reads, fcb);
        return 0;
    }
    if (strcmp(tok[1], "link") == 0) {
        int reads = 0;
        if (insert_link(n, pos, &reads) != 0) {
            return fail("bad-number");
        }
        printf("insert link n=%d pos=%d reads=%d\n", n, pos, reads);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_linkmax(char tok[][TOK_W], int ntok) {
    unsigned long long block;
    unsigned long long ptr;
    uint64_t bytes = 0;

    if (need(ntok, 3) || take_u64(tok[1], 2, 65536, &block) || take_u64(tok[2], 1, 4, &ptr)) {
        return 1;
    }
    if (link_max(block, ptr, &bytes) != 0) {
        return fail("bad-number");
    }
    printf("linkmax data=%llu bytes=%llu", (unsigned long long)(block - ptr),
           (unsigned long long)bytes);
    if (bytes % (1ull << 30) == 0) {
        printf(" gb=%llu", (unsigned long long)(bytes >> 30));
    }
    printf("\n");
    return 0;
}

static int cmd_fat(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "limit") == 0) {
        int entry;
        unsigned long long cluster;
        uint64_t fat_bytes = 0;
        uint64_t file_bytes = 0;
        if (need(ntok, 4) || take_i(tok[2], 1, 4, &entry) || take_u64(tok[3], 1, 1048576, &cluster)) {
            return 1;
        }
        if (fat_limit(entry, cluster, &fat_bytes, &file_bytes) != 0) {
            return fail("bad-number");
        }
        printf("fat entries=%llu fat-bytes=%llu file-bytes=%llu\n",
               (unsigned long long)(fat_bytes / (uint64_t)entry), (unsigned long long)fat_bytes,
               (unsigned long long)file_bytes);
        return 0;
    }
    if (strcmp(tok[1], "nth") == 0) {
        unsigned long long cluster;
        unsigned long long byte_no;
        int chain[8];
        int nchain;
        int i;
        int nth = 0;
        int found = 0;
        if (ntok < 5 || ntok > 12) {
            return fail("bad-command");
        }
        if (take_u64(tok[2], 1, 1048576, &cluster) || take_u64(tok[3], 1, ~0ull, &byte_no)) {
            return 1;
        }
        nchain = ntok - 4;
        for (i = 0; i < nchain; i++) {
            if (take_i(tok[4 + i], 0, 1000000, &chain[i])) {
                return 1;
            }
        }
        if (fat_nth(cluster, byte_no, chain, nchain, &nth, &found) != 0) {
            return soft("range");
        }
        printf("fat byte=%llu nth=%d cluster=%d\n", byte_no, nth, found);
        return 0;
    }
    return fail("bad-command");
}

static int group_fill_cmd(Group *g, char tok[][TOK_W], int ntok) {
    int block;
    int count;
    int first;
    int step;

    if (need(ntok, 6) || take_i(tok[2], 0, GROUP_DISK - 1, &block) ||
        take_i(tok[3], 1, GROUP_CAP, &count) || take_i(tok[4], 0, GROUP_DISK - 1, &first) ||
        take_i(tok[5], -(GROUP_DISK - 1), GROUP_DISK - 1, &step)) {
        return 1;
    }
    if (group_fill(g, block, count, first, step) != 0) {
        return fail("bad-number");
    }
    printf("fill block=%d n=%d bottom=%d top=%d\n", block, count, g->db[block][0],
           g->db[block][count - 1]);
    return 0;
}

static int group_stack_cmd(Group *g, char tok[][TOK_W], int ntok) {
    int blk[GROUP_CAP];
    int n = ntok - 2;
    int i;

    if (n < 1 || n > g->cap) {
        return fail("bad-command");
    }
    for (i = 0; i < n; i++) {
        if (take_i(tok[2 + i], 0, GROUP_DISK - 1, &blk[i])) {
            return 1;
        }
    }
    if (group_set_stack(g, blk, n) != 0) {
        return fail("bad-number");
    }
    printf("stack n=%d bottom=%d top=%d\n", g->n, g->blk[0], g->blk[g->n - 1]);
    return 0;
}

static int group_alloc_cmd(Group *g, char tok[][TOK_W], int ntok) {
    int block = 0;
    int did = 0;
    int rc;

    if (need(ntok, 2)) {
        return 1;
    }
    (void)tok;
    rc = group_alloc(g, &block, &did);
    if (rc == -2) {
        return soft("none");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    printf("alloc block=%d read=%d n=%d\n", block, did, g->n);
    return 0;
}

static int group_free_cmd(Group *g, char tok[][TOK_W], int ntok) {
    int block;
    int did = 0;

    if (need(ntok, 3) || take_i(tok[2], 0, GROUP_DISK - 1, &block)) {
        return 1;
    }
    if (group_free(g, block, &did) != 0) {
        return fail("bad-number");
    }
    printf("free block=%d write=%d n=%d\n", block, did, g->n);
    return 0;
}

static int cmd_group(Group *g, char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "cap") == 0) {
        int cap;
        if (need(ntok, 3) || take_i(tok[2], 1, GROUP_CAP, &cap)) {
            return 1;
        }
        group_init(g, cap);
        printf("group cap=%d\n", g->cap);
        return 0;
    }
    if (strcmp(tok[1], "fill") == 0) {
        return group_fill_cmd(g, tok, ntok);
    }
    if (strcmp(tok[1], "stack") == 0) {
        return group_stack_cmd(g, tok, ntok);
    }
    if (strcmp(tok[1], "total") == 0) {
        int total;
        if (need(ntok, 2)) {
            return 1;
        }
        total = group_total(g);
        if (total < 0) {
            return fail("bad-number");
        }
        printf("total n=%d\n", total);
        return 0;
    }
    if (strcmp(tok[1], "alloc") == 0) {
        return group_alloc_cmd(g, tok, ntok);
    }
    if (strcmp(tok[1], "free") == 0) {
        return group_free_cmd(g, tok, ntok);
    }
    if (strcmp(tok[1], "show") == 0) {
        if (need(ntok, 2)) {
            return 1;
        }
        if (g->n <= 0) {
            printf("stack n=0\n");
        } else {
            printf("stack n=%d bottom=%d top=%d\n", g->n, g->blk[0], g->blk[g->n - 1]);
        }
        return 0;
    }
    return fail("bad-command");
}

static int cmd_words(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "exam") == 0) {
        if (need(ntok, 2)) {
            return 1;
        }
        if (exam_print(tok[1]) != 0) {
            return fail("bad-exam");
        }
        return 0;
    }
    if (strcmp(tok[0], "claim") == 0) {
        if (need(ntok, 2)) {
            return 1;
        }
        if (claim_print(tok[1]) != 0) {
            return fail("bad-claim");
        }
        return 0;
    }
    if (strcmp(tok[0], "card") == 0) {
        if (need(ntok, 1)) {
            return 1;
        }
        card_print();
        return 0;
    }
    return fail("bad-command");
}

int cmd_calc(Group *group, char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "phys") == 0) {
        return cmd_phys(tok, ntok);
    }
    if (strcmp(tok[0], "inode") == 0) {
        return cmd_inode(tok, ntok);
    }
    if (strcmp(tok[0], "bitmap") == 0) {
        return cmd_bitmap(tok, ntok);
    }
    if (strcmp(tok[0], "insert") == 0) {
        return cmd_insert(tok, ntok);
    }
    if (strcmp(tok[0], "linkmax") == 0) {
        return cmd_linkmax(tok, ntok);
    }
    if (strcmp(tok[0], "fat") == 0) {
        return cmd_fat(tok, ntok);
    }
    if (strcmp(tok[0], "group") == 0) {
        return cmd_group(group, tok, ntok);
    }
    return cmd_words(tok, ntok);
}
