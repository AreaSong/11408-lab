#include "exam.h"

#include "bitmap.h"
#include "inode.h"
#include "phys.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *id;
    char letter;
} Letter;

typedef struct {
    const char *name;
    const char *value;
} Claim;

static const Letter LETTERS[] = {
    {"2009-28", 'B'},
    {"2013-24", 'A'},
    {"2013-26", 'A'},
    {"2019-26", 'B'},
    {"2020-24", 'A'},
    {"2024-26", 'A'},
};

static const Claim CLAIMS[] = {
    {"link-random", "false"},
    {"fat-random", "true"},
    {"cont-random", "true"},
    {"index-random", "true"},
    {"cont-extend", "false"},
    {"link-extend", "true"},
    {"index-extend", "true"},
    {"cont-frag", "external"},
    {"link-frag", "none"},
    {"index-frag", "none"},
    {"fat-free", "true"},
    {"inode-free", "false"},
    {"bitmap-grows", "false"},
    {"group-grows", "true"},
    {"chain-grows", "true"},
    {"link-implicit", "true"},
};

static int print_letter(const char *id) {
    int i;
    int n = (int)(sizeof LETTERS / sizeof LETTERS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(LETTERS[i].id, id) == 0) {
            printf("exam %s answer=%c\n", id, LETTERS[i].letter);
            return 0;
        }
    }
    return -1;
}

static int exam_2010_30(void) {
    InodeSpec spec = {256, 4, 4, 2, 1, 0};
    InodeMax max;

    if (inode_max(&spec, &max) != 0 || max.bytes % 1024 != 0) {
        return -1;
    }
    printf("exam 2010-30 answer=C bytes=%llu kb=%llu\n",
           (unsigned long long)max.bytes, (unsigned long long)(max.bytes / 1024));
    return 0;
}

static int exam_2010_45(void) {
    /* 解答按 32 位的字。16384 位正好是 2KB，第（2）（3）问是磁盘调度。 */
    uint64_t bits = 16384;
    uint64_t words = bits / 32;
    uint64_t bytes = words * 4;

    printf("exam 2010-45 part=1 method=bitmap bits=%llu words=%llu bytes=%llu\n",
           (unsigned long long)bits, (unsigned long long)words, (unsigned long long)bytes);
    return 0;
}

static int exam_2011_46(void) {
    printf("exam 2011-46 org=cont fcb=start+count place=together\n");
    return 0;
}

static int exam_2012_46(void) {
    uint64_t blocks = (4ull << 40) / 1024ull;
    int num_bytes = 4;
    int entries;
    uint64_t max_bytes;
    uint64_t pre;
    int direct_n;
    uint64_t part2;

    if (blocks != (1ull << 32)) {
        return -1;
    }
    entries = 512 / num_bytes;
    max_bytes = (uint64_t)entries * 1024ull;
    /* 块数占 2B，只能先表示 2^16 个连续块；剩下 504B 仍是 6B 一个直接索引。 */
    pre = (1ull << 16) * 1024ull;
    direct_n = 504 / 6;
    part2 = pre + (uint64_t)direct_n * 1024ull;
    printf("exam 2012-46 num-bytes=%d entries=%d max-bytes=%llu kb=%llu "
           "part2-bytes=%llu part2-kb=%llu pairs=4+4,1+7,2+6,3+5\n",
           num_bytes, entries, (unsigned long long)max_bytes,
           (unsigned long long)(max_bytes / 1024), (unsigned long long)part2,
           (unsigned long long)(part2 / 1024));
    return 0;
}

static int exam_2014_27(void) {
    uint64_t blocks;
    uint64_t map_bytes;
    uint64_t map_blocks;

    if (bitmap_size(10ull << 30, 4096, &blocks, &map_bytes, &map_blocks) != 0) {
        return -1;
    }
    printf("exam 2014-27 answer=A blocks=%llu map-bytes=%llu map-clusters=%llu\n",
           (unsigned long long)blocks, (unsigned long long)map_bytes,
           (unsigned long long)map_blocks);
    return 0;
}

static int exam_2014_46(void) {
    InsertInfo info;
    int link_reads = 0;
    uint64_t bytes = 0;

    if (insert_cont(200, 30, &info) != 0 || insert_link(200, 30, &link_reads) != 0) {
        return -1;
    }
    if (link_max(1024, 4, &bytes) != 0 || bytes % (1ull << 30) != 0) {
        return -1;
    }
    printf("exam 2014-46 cont=%d fcb=start+length link=%d data=1020 bytes=%llu gb=%llu\n",
           info.reads, link_reads, (unsigned long long)bytes,
           (unsigned long long)(bytes >> 30));
    return 0;
}

static int exam_2015_29(void) {
    InodeSpec spec = {1024, 4, 10, 1, 1, 0};
    InodeHit a;
    InodeHit b;

    if (inode_hit(&spec, 1234, &a) != 0 || inode_hit(&spec, 307400, &b) != 0) {
        return -1;
    }
    printf("exam 2015-29 answer=B a-reads=%d b-reads=%d\n", a.data_reads, b.data_reads);
    return 0;
}

static int exam_2016_47(void) {
    int chain[3] = {100, 106, 108};
    int nth = 0;
    int cluster = 0;
    uint64_t fat_bytes = 0;
    uint64_t file_bytes = 0;

    if (fat_limit(2, 4096, &fat_bytes, &file_bytes) != 0) {
        return -1;
    }
    if (fat_nth(4096, 5000, chain, 3, &nth, &cluster) != 0) {
        return -1;
    }
    /* 48 是题图里 dir1 的起始簇。dir 和 FAT 已在内存，所以不再读它们。 */
    printf("exam 2016-47 fat-bytes=%llu file-bytes=%llu slot100=106 slot106=108 clusters=48+%d\n",
           (unsigned long long)fat_bytes, (unsigned long long)file_bytes, cluster);
    return 0;
}

static int exam_2018_46(void) {
    InodeSpec spec = {4096, 4, 8, 1, 1, 1};
    InodeMax max;
    InodeCover f1;
    InodeCover f2;
    char expr[64];
    uint64_t inodes;
    uint64_t per;
    uint64_t data_files;
    uint64_t images;

    if (inode_max(&spec, &max) != 0 || inode_expr(&max, expr, (int)sizeof expr) != 0) {
        return -1;
    }
    if (inode_cover(&spec, 6ull * 1024, &f1) != 0 || inode_cover(&spec, 40ull * 1024, &f2) != 0) {
        return -1;
    }
    inodes = (1ull << 20) * 4096ull / 64ull;
    per = (5600ull + 4095ull) / 4096ull;
    data_files = (512ull << 20) / per;
    images = inodes < data_files ? inodes : data_files;
    printf("exam 2018-46 max-bytes=%llu expr=%s inodes=%llu data-files=%llu images=%llu "
           "f1-addr=%d f2-addr=%d same=%s\n",
           (unsigned long long)max.bytes, expr, (unsigned long long)inodes,
           (unsigned long long)data_files, (unsigned long long)images, f1.addr_reads,
           f2.addr_reads, f1.addr_reads == f2.addr_reads ? "yes" : "no");
    return 0;
}

static int exam_2022_45(void) {
    InodeSpec spec = {4096, 4, 10, 1, 1, 1};
    InodeCover cover;

    if (inode_cover(&spec, 6ull << 20, &cover) != 0) {
        return -1;
    }
    printf("exam 2022-45 blocks=%llu indirect=%s\n",
           (unsigned long long)cover.blocks, cover.indirect);
    return 0;
}

int exam_print(const char *id) {
    if (strcmp(id, "2010-30") == 0) {
        return exam_2010_30();
    }
    if (strcmp(id, "2010-45") == 0) {
        return exam_2010_45();
    }
    if (strcmp(id, "2011-46") == 0) {
        return exam_2011_46();
    }
    if (strcmp(id, "2012-46") == 0) {
        return exam_2012_46();
    }
    if (strcmp(id, "2014-27") == 0) {
        return exam_2014_27();
    }
    if (strcmp(id, "2014-46") == 0) {
        return exam_2014_46();
    }
    if (strcmp(id, "2015-29") == 0) {
        return exam_2015_29();
    }
    if (strcmp(id, "2016-47") == 0) {
        return exam_2016_47();
    }
    if (strcmp(id, "2018-46") == 0) {
        return exam_2018_46();
    }
    if (strcmp(id, "2022-45") == 0) {
        return exam_2022_45();
    }
    return print_letter(id);
}

int claim_print(const char *name) {
    int i;
    int n = (int)(sizeof CLAIMS / sizeof CLAIMS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(CLAIMS[i].name, name) == 0) {
            printf("claim %s value=%s\n", CLAIMS[i].name, CLAIMS[i].value);
            return 0;
        }
    }
    return -1;
}

void card_print(void) {
    printf("card cont=random,external-frag,hard-extend\n");
    printf("card link=no-random,no-external-frag,pointer-in-block\n");
    printf("card fat=random,resident,no-external-frag\n");
    printf("card index=random,no-external-frag,index-block\n");
    printf("card layout=boot,super,free,inode,data\n");
    printf("card open=sys-one-per-inode,user-per-fd\n");
    printf("card vfs=one-ops-two-fs\n");
    printf("card mount=path-crosses-fs\n");
}
