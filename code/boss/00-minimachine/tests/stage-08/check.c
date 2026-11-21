#include "common/mm_machine.h"
#include "fs/fs_free.h"
#include "fs/fs_ftl.h"
#include "fs/fs_geom.h"
#include "fs/fs_index.h"
#include "fs/fs_raid.h"
#include "fs/fs_sched.h"
#include "fs/fs_vfs.h"
#include "kernel/k_file.h"
#include "kernel/k_proc.h"
#include "kernel/k_vm.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int has_step(const FsNs *ns, const char *frag) {
    int i;
    for (i = 0; i < fs_ns_nstep(ns); i++) {
        if (strstr(fs_ns_step(ns, i), frag) != NULL) {
            return 1;
        }
    }
    return 0;
}

static void test_sched(void) {
    static const int six[] = {98, 183, 37, 122, 14, 124, 65, 67};
    static const int y2015[] = {130, 42, 180, 15, 199};
    static const int y2024[] = {300, 120, 110, 0, 160, 210, 399};
    static const int y2021[] = {184, 187, 176, 182, 199};
    static const int y2010[] = {50, 90, 30, 120};
    static const int y2009[] = {35, 45, 12, 68, 110, 180, 170, 195};
    static const int tie[] = {60, 40};
    FsSched out;
    CHECK(fs_sched("fcfs", 53, 0, 1, 199, six, 8, &out) == 0);
    CHECK_EQ(out.move, 640);
    CHECK(fs_sched("sstf", 53, 0, 0, 0, six, 8, &out) == 0);
    CHECK_EQ(out.move, 236);
    CHECK(fs_sched("scan", 53, 1, 1, 199, six, 8, &out) == 0);
    CHECK_EQ(out.move, 331);
    CHECK(fs_sched("look", 53, 1, 1, 199, six, 8, &out) == 0);
    CHECK_EQ(out.move, 299);
    CHECK(fs_sched("cscan", 53, 1, 1, 199, six, 8, &out) == 0);
    CHECK_EQ(out.move, 382);
    CHECK_EQ(out.track[6], 14);
    CHECK(fs_sched("clook", 53, 1, 0, 0, six, 8, &out) == 0);
    CHECK_EQ(out.move, 322);
    CHECK(fs_sched("scan", 58, 1, 1, 199, y2015, 5, &out) == 0);
    CHECK_EQ(out.move, 325);
    CHECK_EQ(out.track[0], 130);
    CHECK_EQ(out.track[4], 15);
    CHECK(fs_sched("cscan", 200, -1, 1, 399, y2024, 7, &out) == 0);
    CHECK_EQ(out.move, 788);
    CHECK_EQ(out.track[0], 160);
    CHECK_EQ(out.track[4], 399);
    CHECK(fs_sched("sstf", 184, 0, 0, 0, y2021, 5, &out) == 0);
    CHECK_EQ(out.move, 41);
    CHECK_EQ(out.track[0], 184);
    CHECK(fs_sched("clook", 100, 1, 0, 0, y2010, 4, &out) == 0);
    CHECK_EQ(out.move, 170);
    CHECK(fs_sched("look", 105, 1, 0, 0, y2009, 8, &out) == 0);
    CHECK_EQ(out.track[0], 110);
    CHECK_EQ(out.track[7], 12);
    CHECK(fs_sched("sstf", 50, 0, 0, 0, tie, 2, &out) == 0);
    CHECK_EQ(out.track[0], 40);
    CHECK_EQ(out.move, 30);
}

static void test_time(void) {
    FsTime part;
    char ms[32];
    char one[32];
    long long sum;
    int cyl = 0;
    int head = 0;
    int sector = 0;
    int order[4];
    static const int clusters[] = {100260, 60005, 101660, 110560};
    CHECK(fs_time_each(8000, 7200, 1000, 1, 0, &part) == 0);
    sum = part.seek + part.rot + part.xfer + part.ctrl;
    CHECK(fs_fmt_ms(sum, 3, ms, (int)sizeof ms) == 0);
    CHECK(fs_fmt_ms(sum, 1, one, (int)sizeof one) == 0);
    CHECK_STR(ms, "12.175");
    CHECK_STR(one, "12.2");
    CHECK(fs_time_rate(6000, 10000, 4000, 20000000, 200, &part) == 0);
    sum = part.seek + part.rot + part.xfer + part.ctrl;
    CHECK(fs_fmt_ms(sum, 1, one, (int)sizeof one) == 0);
    CHECK_STR(one, "9.4");
    CHECK(fs_time_sum(170000, 6000, 100, 4, 0, &part) == 0);
    sum = part.seek + part.rot + part.xfer + part.ctrl;
    CHECK(fs_fmt_ms(sum, 3, ms, (int)sizeof ms) == 0);
    CHECK_STR(ms, "190.400");
    CHECK_EQ(fs_disk_bytes(300, 10, 200, 512), 307200000LL);
    CHECK(fs_cluster_sstf(10, 200, 2, 85, clusters, 4, order) == 0);
    CHECK_EQ(order[0], 100260);
    CHECK_EQ(order[1], 101660);
    CHECK_EQ(order[2], 110560);
    CHECK_EQ(order[3], 60005);
    CHECK(fs_cluster_chs(10, 200, 2, 100530, &cyl, &head, &sector) == 0);
    CHECK_EQ(cyl, 100);
    CHECK_EQ(head, 5);
    CHECK_EQ(sector, 60);
}

static FsInodeSpec spec_of(uint64_t block, uint64_t addr, int d, int s, int w, int t) {
    FsInodeSpec spec;
    spec.block_size = block;
    spec.addr_size = addr;
    spec.direct = d;
    spec.single = s;
    spec.doubly = w;
    spec.triply = t;
    return spec;
}

static void test_inode(void) {
    FsInodeSpec spec;
    FsInodeMax max;
    FsInodeHit hit;
    FsInodeCover cover;
    FsInsert ins;
    char expr[64];
    uint64_t bytes = 0;
    uint64_t blocks = 0;
    uint64_t map_bytes = 0;
    uint64_t map_blocks = 0;
    int reads = 0;
    int next = 0;
    spec = spec_of(256, 4, 4, 2, 1, 0);
    CHECK(fs_inode_max(&spec, &max) == 0);
    CHECK(fs_inode_expr(&max, expr, (int)sizeof expr) == 0);
    CHECK_EQ(max.bytes, 1082368LL);
    CHECK_STR(expr, "1KB+32KB+1MB");
    spec = spec_of(1024, 4, 10, 1, 1, 0);
    CHECK(fs_inode_hit(&spec, 1234, &hit) == 0);
    CHECK_EQ(hit.data_reads, 1);
    CHECK_EQ(hit.addr_reads, 0);
    CHECK(fs_inode_hit(&spec, 307400, &hit) == 0);
    CHECK_EQ(hit.data_reads, 3);
    CHECK_EQ(hit.level, 2);
    spec = spec_of(4096, 4, 8, 1, 1, 1);
    CHECK(fs_inode_max(&spec, &max) == 0);
    CHECK(fs_inode_expr(&max, expr, (int)sizeof expr) == 0);
    CHECK_STR(expr, "32KB+4MB+4GB+4TB");
    spec = spec_of(4096, 4, 10, 1, 1, 1);
    CHECK(fs_inode_cover(&spec, 6291456, &cover) == 0);
    CHECK_EQ(cover.blocks, 1536);
    CHECK_STR(cover.indirect, "1+2");
    CHECK(fs_insert_cont(200, 30, &ins) == 0);
    CHECK_EQ(ins.move, 29);
    CHECK_EQ(ins.reads, 59);
    CHECK(fs_insert_link(200, 30, &reads) == 0);
    CHECK_EQ(reads, 31);
    CHECK(fs_link_max(1024, 4, &bytes) == 0);
    CHECK_EQ(bytes, 4380866641920LL);
    CHECK(fs_bitmap_size(10737418240ULL, 4096, &blocks, &map_bytes, &map_blocks) == 0);
    CHECK_EQ(blocks, 2621440LL);
    CHECK_EQ(map_blocks, 80LL);
    CHECK(fs_phys_visit(FS_PHYS_FAT, 8, &reads, &next) == 0);
    CHECK_EQ(reads, 1);
    CHECK(fs_phys_visit(FS_PHYS_LINK, 8, &reads, &next) == 0);
    CHECK_EQ(reads, 8);
}

static void test_free(void) {
    static FsGroup group;
    uint64_t word = 0;
    uint64_t bit = 0;
    uint64_t block = 0;
    int got = 0;
    int did = 0;
    int stack[2];
    CHECK(fs_bitmap_locate(100, 32, 1, &word, &bit) == 0);
    CHECK_EQ(word, 4);
    CHECK_EQ(bit, 4);
    CHECK(fs_bitmap_locate(13, 16, 0, &word, &bit) == 0);
    CHECK_EQ(word, 0);
    CHECK_EQ(bit, 13);
    CHECK(fs_bitmap_compose(2, 5, 32, 1, &block) == 0);
    CHECK_EQ(block, 37);
    fs_group_init(&group, 3);
    CHECK(fs_group_fill(&group, 10, 3, 20, 1) == 0);
    stack[0] = 10;
    stack[1] = 11;
    CHECK(fs_group_set_stack(&group, stack, 2) == 0);
    CHECK_EQ(fs_group_total(&group), 5);
    CHECK(fs_group_alloc(&group, &got, &did) == 0);
    CHECK_EQ(got, 11);
    CHECK_EQ(did, 0);
    CHECK(fs_group_alloc(&group, &got, &did) == 0);
    CHECK_EQ(got, 10);
    CHECK_EQ(did, 1);
    CHECK(fs_group_alloc(&group, &got, &did) == 0);
    CHECK_EQ(got, 22);
    CHECK_EQ(did, 0);
    CHECK(fs_group_free(&group, 30, &did) == 0);
    CHECK_EQ(did, 0);
    CHECK_EQ(group.n, 3);
    CHECK(fs_group_free(&group, 31, &did) == 0);
    CHECK_EQ(did, 1);
    CHECK_EQ(group.n, 1);
    CHECK_EQ(group.blk[0], 31);
}

static void test_ftl_raid(void) {
    int erase[2];
    int host = 0;
    int flash = 0;
    FsRaid raid;
    CHECK(fs_ftl_wear(2, 2, 8, 0, erase, &host, &flash) == 0);
    CHECK_EQ(host, 8);
    CHECK_EQ(flash, 8);
    CHECK_EQ(erase[0], 2);
    CHECK_EQ(erase[1], 0);
    CHECK(fs_ftl_wear(2, 2, 8, 1, erase, &host, &flash) == 0);
    CHECK_EQ(erase[0], 1);
    CHECK_EQ(erase[1], 1);
    CHECK(fs_raid_plan(0, 3, 2, &raid) == 0);
    CHECK_EQ(raid.usable, 6);
    CHECK_EQ(raid.tolerate, 0);
    CHECK_STR(raid.cell[0][0], "D0");
    CHECK_STR(raid.cell[0][2], "D2");
    CHECK(fs_raid_plan(1, 2, 2, &raid) == 0);
    CHECK_EQ(raid.usable, 2);
    CHECK_EQ(raid.tolerate, 1);
    CHECK_STR(raid.cell[0][0], "D0");
    CHECK_STR(raid.cell[0][1], "D0");
    CHECK(fs_raid_plan(5, 3, 3, &raid) == 0);
    CHECK_EQ(raid.usable, 6);
    CHECK_EQ(raid.parity, 3);
    CHECK_EQ(raid.tolerate, 1);
    CHECK_STR(raid.cell[0][2], "P");
    CHECK_STR(raid.cell[1][1], "P");
    CHECK_STR(raid.cell[2][0], "P");
    CHECK_STR(raid.cell[0][0], "D0");
    CHECK_STR(raid.cell[2][2], "D5");
}

static void test_links(void) {
    FsNs *ns = fs_ns_new();
    FsOut out;
    CHECK(ns != NULL);
    CHECK(fs_ns_create(ns, "/F1", "data", &out) == FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK(fs_ns_symlink(ns, "/F2", "/F1", &out) == FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK(fs_ns_link(ns, "/F3", "/F1", &out) == FS_OK);
    CHECK_EQ(out.nlink, 2);
    CHECK(fs_ns_unlink(ns, "/F1", &out) == FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(out.kept, 1);
    CHECK(fs_ns_stat(ns, "/F2", 0, &out) == FS_OK);
    CHECK_EQ(out.kind, 3);
    CHECK_EQ(out.nlink, 1);
    CHECK(fs_ns_stat(ns, "/F2", 1, &out) == FS_DANGLE);
    CHECK(fs_ns_stat(ns, "/F3", 1, &out) == FS_OK);
    CHECK_EQ(out.kind, 2);
    CHECK_EQ(out.nlink, 1);
    CHECK_STR(out.text, "data");
    CHECK(fs_ns_symlink(ns, "/loop", "/loop", &out) == FS_OK);
    CHECK(fs_ns_stat(ns, "/loop", 1, &out) == FS_LOOP);
    fs_ns_free(ns);
}

static void test_open(void) {
    FsNs *ns = fs_ns_new();
    FsOut out;
    CHECK(fs_ns_mkdir(ns, "/a", &out) == FS_OK);
    CHECK_EQ(out.nlink, 2);
    CHECK(fs_ns_mkdir(ns, "/a/b", &out) == FS_OK);
    CHECK(fs_ns_create(ns, "/a/b/c", "hi", &out) == FS_OK);
    CHECK(fs_ns_open(ns, 0, "/a/b/c", 1, &out) == FS_OK);
    CHECK_EQ(out.reads, 6);
    CHECK_EQ(out.fd, 0);
    CHECK(fs_ns_pread(ns, 0, 0, 0, &out) == FS_OK);
    CHECK_EQ(out.reads, 1);
    CHECK(has_step(ns, "read-data"));
    CHECK(!has_step(ns, "read-dir"));
    CHECK(fs_ns_pread(ns, 0, 0, 0, &out) == FS_OK);
    CHECK_EQ(out.reads, 1);
    CHECK(fs_ns_cd(ns, "/a/b", &out) == FS_OK);
    CHECK_EQ(out.reads, 4);
    CHECK(fs_ns_open(ns, 0, "c", 1, &out) == FS_OK);
    CHECK_EQ(out.reads, 2);
    CHECK(fs_ns_create(ns, "/F", "x", &out) == FS_OK);
    CHECK(fs_ns_open(ns, 0, "/F", 1, &out) == FS_OK);
    CHECK_EQ(out.fd, 2);
    CHECK(fs_ns_open(ns, 1, "/F", 2, &out) == FS_OK);
    CHECK_EQ(out.fd, 0);
    CHECK_EQ(out.count, 2);
    CHECK(fs_ns_pread(ns, 0, 2, 0, &out) == FS_OK);
    CHECK_EQ(out.offset, 1);
    CHECK(fs_ns_pread(ns, 1, 0, 0, &out) == FS_ACCES);
    CHECK_EQ(out.offset, 0);
    CHECK(fs_ns_close(ns, 0, 2, &out) == FS_OK);
    CHECK_EQ(out.count, 1);
    CHECK_EQ(out.kept, 1);
    CHECK_EQ(out.nlink, 1);
    CHECK(fs_ns_close(ns, 1, 0, &out) == FS_OK);
    CHECK_EQ(out.count, 0);
    CHECK_EQ(out.kept, 0);
    fs_ns_free(ns);
}

static void test_mount_index(void) {
    FsNs *ns = fs_ns_new();
    FsOut out;
    char kind[8];
    char path[16];
    int keys = 0;
    int height = 0;
    int i;
    FILE *fp;
    unsigned char magic[4];
    CHECK(fs_ns_mount(ns, &out) == FS_OK);
    CHECK(fs_ns_create(ns, "/mnt/a", "hi", &out) == FS_OK);
    CHECK_STR(out.text, "hi");
    CHECK(fs_ns_kind(ns, "/mnt/a", kind, (int)sizeof kind) == FS_OK);
    CHECK_STR(kind, "fat");
    CHECK(fs_ns_create(ns, "/b", "no", &out) == FS_OK);
    CHECK(fs_ns_kind(ns, "/b", kind, (int)sizeof kind) == FS_OK);
    CHECK_STR(kind, "inode");
    CHECK(fs_ns_link(ns, "/h", "/mnt/a", &out) == FS_BAD);
    CHECK(fs_ns_mkdir(ns, "/d", &out) == FS_OK);
    for (i = 0; i < 6; i++) {
        snprintf(path, sizeof path, "/d/a%d", i);
        CHECK(fs_ns_create(ns, path, "z", &out) == FS_OK);
    }
    CHECK(fs_ns_index(ns, "/d", &keys, &height) == FS_OK);
    CHECK_EQ(keys, 6);
    CHECK(height >= 1);
    CHECK(fs_ns_open(ns, 0, "/d/a5", 1, &out) == FS_OK);
    CHECK_EQ(out.reads, 4);
    CHECK(has_step(ns, "index=bp"));
    CHECK(fs_ns_save(ns, "/tmp/lab-mm-fs08.img", &out) == FS_OK);
    CHECK_EQ(out.extra, fs_ns_bytes(ns));
    fp = fopen("/tmp/lab-mm-fs08.img", "rb");
    CHECK(fp != NULL);
    CHECK(fp != NULL && fread(magic, 1, 4, fp) == 4);
    CHECK(memcmp(magic, "MMFS", 4) == 0);
    if (fp != NULL) {
        fclose(fp);
    }
    remove("/tmp/lab-mm-fs08.img");
    fs_ns_free(ns);
}

static void test_slot(void) {
    MmMachine *machine = mm_create(1u << 20);
    FsOut out;
    char buf[8];
    CHECK(machine != NULL);
    CHECK(k_proc_on(machine) == 0);
    CHECK(k_demand_on(machine, 4, "fifo") == 0);
    CHECK(k_fs_on(machine) == 0);
    CHECK(fs_ns_create(k_fs_ns(machine), "/t", "hi", &out) == FS_OK);
    CHECK_EQ(out.ino, 1);
    CHECK(k_fs_fill_slot(machine, 1) == 0);
    CHECK_EQ(k_fs_staged(machine, 1, buf, (int)sizeof buf), 2);
    CHECK_STR(buf, "hi");
    mm_destroy(machine);
}

int main(void) {
    test_sched();
    test_time();
    test_inode();
    test_free();
    test_ftl_raid();
    test_links();
    test_open();
    test_mount_index();
    test_slot();
    return TEST_SUMMARY();
}
