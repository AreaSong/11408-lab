#include "bitmap.h"
#include "exam.h"
#include "group.h"
#include "image.h"
#include "inode.h"
#include "mount.h"
#include "phys.h"
#include "test.h"
#include "vfs.h"

static void test_phys(void) {
    int reads = 0;
    int next = 0;
    InsertInfo info;
    uint64_t bytes = 0;

    CHECK_EQ(phys_visit(PHYS_LINK, 5, &reads, &next), 0);
    CHECK_EQ(reads, 5);
    CHECK_EQ(next, 1);
    CHECK_EQ(phys_visit(PHYS_INDEX, 50, &reads, &next), 0);
    CHECK_EQ(reads, 2);
    CHECK_EQ(phys_visit(PHYS_CONT, 50, &reads, &next), 0);
    CHECK_EQ(reads, 1);
    CHECK_EQ(phys_visit(PHYS_FAT, 50, &reads, &next), 0);
    CHECK_EQ(reads, 1);
    CHECK_EQ(phys_visit(PHYS_FAT_DISK, 50, &reads, &next), 0);
    CHECK_EQ(reads, 2);
    CHECK_EQ(insert_cont(200, 30, &info), 0);
    CHECK_EQ(info.reads, 59);
    CHECK_EQ(info.move, 29);
    CHECK_EQ(info.head, 1);
    CHECK_EQ(insert_link(200, 30, &reads), 0);
    CHECK_EQ(reads, 31);
    CHECK_EQ(insert_link(8, 1, &reads), 0);
    CHECK_EQ(reads, 1);
    CHECK_EQ(insert_cont(200, 200, &info), 0);
    CHECK_EQ(info.head, 0);
    CHECK_EQ(info.reads, 3);
    CHECK_EQ(link_max(1024, 4, &bytes), 0);
    CHECK_EQ(bytes, 1020ull << 32);
    CHECK_EQ(bytes >> 30, 4080);
}

static void test_inode(void) {
    InodeSpec spec = {256, 4, 4, 2, 1, 0};
    InodeMax max;
    InodeHit hit;
    InodeCover cover;
    char expr[80];
    uint64_t expect = (32ull << 10) + (4ull << 20) + (4ull << 30) + (4ull << 40);

    CHECK_EQ(inode_max(&spec, &max), 0);
    CHECK_EQ(max.bytes, (4 + 2 * 64 + 64 * 64) * 256);
    CHECK_EQ(max.bytes / 1024, 1057);
    spec = (InodeSpec){1024, 4, 10, 1, 1, 0};
    CHECK_EQ(inode_hit(&spec, 1234, &hit), 0);
    CHECK_EQ(hit.data_reads, 1);
    CHECK_EQ(hit.level, 0);
    CHECK_EQ(inode_hit(&spec, 307400, &hit), 0);
    CHECK_EQ(hit.logical, 300);
    CHECK_EQ(hit.level, 2);
    CHECK_EQ(hit.data_reads, 3);
    CHECK_EQ(inode_hit(&spec, 10239, &hit), 0);
    CHECK_EQ(hit.data_reads, 1);
    CHECK_EQ(inode_hit(&spec, 10240, &hit), 0);
    CHECK_EQ(hit.data_reads, 2);
    CHECK_EQ(inode_hit(&spec, 272383, &hit), 0);
    CHECK_EQ(hit.level, 1);
    CHECK_EQ(inode_hit(&spec, 272384, &hit), 0);
    CHECK_EQ(hit.level, 2);
    CHECK_EQ(hit.data_reads, 3);
    spec = (InodeSpec){4096, 4, 8, 1, 1, 1};
    CHECK_EQ(inode_max(&spec, &max), 0);
    CHECK_EQ(max.bytes, expect);
    CHECK_EQ(inode_expr(&max, expr, (int)sizeof expr), 0);
    CHECK_STR(expr, "32KB+4MB+4GB+4TB");
    CHECK_EQ(inode_cover(&spec, 6u * 1024u, &cover), 0);
    CHECK_EQ(cover.addr_reads, 0);
    CHECK_STR(cover.indirect, "none");
    CHECK_EQ(inode_cover(&spec, 40u * 1024u, &cover), 0);
    CHECK_EQ(cover.blocks, 10);
    CHECK_EQ(cover.addr_reads, 1);
    CHECK_STR(cover.indirect, "1");
    spec = (InodeSpec){4096, 4, 10, 1, 1, 1};
    CHECK_EQ(inode_cover(&spec, 6ull << 20, &cover), 0);
    CHECK_EQ(cover.blocks, 1536);
    CHECK_STR(cover.indirect, "1+2");
}

static void test_bitmap(void) {
    uint64_t blocks = 0;
    uint64_t map_bytes = 0;
    uint64_t map_blocks = 0;
    uint64_t word = 0;
    uint64_t bit = 0;
    uint64_t block = 0;

    CHECK_EQ(bitmap_size(10ull << 30, 4096, &blocks, &map_bytes, &map_blocks), 0);
    CHECK_EQ(blocks, 2621440);
    CHECK_EQ(map_bytes, 327680);
    CHECK_EQ(map_blocks, 80);
    CHECK_EQ(bitmap_locate(100, 32, 1, &word, &bit), 0);
    CHECK_EQ(word, 4);
    CHECK_EQ(bit, 4);
    CHECK_EQ(bitmap_compose(2, 5, 32, 1, &block), 0);
    CHECK_EQ(block, 37);
    CHECK_EQ(bitmap_locate(13, 16, 0, &word, &bit), 0);
    CHECK_EQ(word, 0);
    CHECK_EQ(bit, 13);
}

static void test_group(void) {
    static Group group;
    int block = 0;
    int did = 0;
    int stack[2] = {300, 299};

    group_init(&group, 100);
    CHECK_EQ(group_fill(&group, 300, 100, 400, -1), 0);
    CHECK_EQ(group_set_stack(&group, stack, 2), 0);
    CHECK_EQ(group_total(&group), 102);
    CHECK_EQ(group_alloc(&group, &block, &did), 0);
    CHECK_EQ(block, 299);
    CHECK_EQ(did, 0);
    CHECK_EQ(group_alloc(&group, &block, &did), 0);
    CHECK_EQ(block, 300);
    CHECK_EQ(did, 1);
    CHECK_EQ(group.n, 100);
    CHECK_EQ(group_alloc(&group, &block, &did), 0);
    CHECK_EQ(block, 301);
    CHECK_EQ(group.n, 99);
    CHECK_EQ(group_free(&group, 700, &did), 0);
    CHECK_EQ(did, 0);
    CHECK_EQ(group.n, 100);
    CHECK_EQ(group_free(&group, 711, &did), 0);
    CHECK_EQ(did, 1);
    CHECK_EQ(group.blk[0], 711);
    CHECK_EQ(group_free(&group, 703, &did), 0);
    CHECK_EQ(group_free(&group, 788, &did), 0);
    CHECK_EQ(group_free(&group, 701, &did), 0);
    CHECK_EQ(group.n, 4);
    CHECK_EQ(group.blk[0], 711);
    CHECK_EQ(group.blk[1], 703);
    CHECK_EQ(group.blk[2], 788);
    CHECK_EQ(group.blk[3], 701);
    CHECK_EQ(group.dn[711], 100);
    CHECK_EQ(group.db[711][0], 400);
    CHECK_EQ(group.db[711][99], 700);
}

static void test_fat(void) {
    int chain[3] = {100, 106, 108};
    int nth = 0;
    int cluster = 0;
    uint64_t fat = 0;
    uint64_t file = 0;

    CHECK_EQ(fat_limit(2, 4096, &fat, &file), 0);
    CHECK_EQ(fat, 131072);
    CHECK_EQ(file, 268435456);
    CHECK_EQ(fat_nth(4096, 5000, chain, 3, &nth, &cluster), 0);
    CHECK_EQ(nth, 2);
    CHECK_EQ(cluster, 106);
}

static void test_image(void) {
    static Image img;
    int ino = 0;
    int fd = 0;
    int sys = 0;
    int refs = 0;
    int got = 0;
    char buf[16];

    CHECK_EQ(image_format(&img, "/tmp/lab14-unit-image.img", 32), IMG_OK);
    CHECK_EQ(img.free_count, 24);
    CHECK_EQ(image_create(&img, "a", &ino), IMG_OK);
    CHECK_EQ(ino, 1);
    CHECK_EQ(image_replace(&img, "a", "hello"), IMG_OK);
    CHECK_EQ(img.ino[1].size, 5);
    CHECK_EQ(img.free_count, 23);
    CHECK_EQ(image_open(&img, 0, "a", 0, &fd, &sys, &refs), IMG_OK);
    CHECK_EQ(image_open(&img, 1, "a", 0, &fd, &sys, &refs), IMG_OK);
    CHECK_EQ(sys, 0);
    CHECK_EQ(refs, 2);
    CHECK_EQ(image_read(&img, 0, 0, 5, buf, &got), IMG_OK);
    CHECK_STR(buf, "hello");
    CHECK_EQ(image_fd_write(&img, 0, 0, "x"), IMG_EACCES);
    CHECK_EQ(image_load(&img, "/tmp/lab14-unit-image.img"), IMG_OK);
    CHECK_EQ(img.sys_used[0], 0);
    CHECK_EQ(image_find(&img, "a", &ino), IMG_OK);
    CHECK_EQ(img.ino[ino].size, 5);
}

static void test_vfs_mount(void) {
    char out[32];
    static MountSet mounted;
    MountStep steps[8];
    int nsteps = 0;
    int fs = 0;
    int node = 0;

    CHECK_EQ(vfs_mem_roundtrip("hello", out, (int)sizeof out), 0);
    CHECK_STR(out, "hello");
    CHECK_EQ(vfs_disk_roundtrip("/tmp/lab14-unit-vfs.img", "hello", out, (int)sizeof out), 0);
    CHECK_STR(out, "hello");
    mount_reset(&mounted);
    CHECK_EQ(mount_mkdir(&mounted, 0, "/", "mnt"), MNT_OK);
    CHECK_EQ(mount_file(&mounted, 1, "/", "a", "hi"), MNT_OK);
    CHECK_EQ(mount_attach(&mounted, "/mnt"), MNT_OK);
    CHECK_EQ(mount_cat(&mounted, "/mnt/a", steps, 8, &nsteps, &fs, &node), MNT_OK);
    CHECK_EQ(fs, 1);
    CHECK_EQ(steps[0].cross, 1);
    CHECK_STR(mounted.node[fs][node].data, "hi");
}

int main(void) {
    test_phys();
    test_inode();
    test_bitmap();
    test_group();
    test_fat();
    test_image();
    test_vfs_mount();
    CHECK_EQ(exam_print("2099-1"), -1);
    return TEST_SUMMARY();
}
