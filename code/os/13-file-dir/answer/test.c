#include "asl.h"
#include "exam.h"
#include "fs.h"
#include "test.h"

#include <string.h>

static void test_asl(void) {
    Frac frac;
    AslWalk walk;

    CHECK_EQ(asl_seq(10000, &frac), 0);
    CHECK_EQ(frac.num, 10001);
    CHECK_EQ(frac.den, 2);
    CHECK_EQ(asl_index(10, &frac), 0);
    CHECK_EQ(frac.num, 29);
    CHECK_EQ(frac.den, 10);
    CHECK_EQ(asl_index(10000, &frac), 0);
    CHECK_EQ(frac.num, 123631);
    CHECK_EQ(frac.den, 10000);
    CHECK_EQ(asl_isam(10000, 100, 0, &frac), 0);
    CHECK_EQ(frac.num, 101);
    CHECK_EQ(frac.den, 1);
    CHECK_EQ(asl_isam(10000, 100, 1, &frac), 0);
    CHECK_EQ(frac.num, 563);
    CHECK_EQ(frac.den, 10);
    CHECK_EQ(asl_isam(10, 3, 0, &frac), 0);
    CHECK_EQ(frac.num, 41);
    CHECK_EQ(frac.den, 10);
    CHECK_EQ(asl_isam(1, 1, 0, &frac), 0);
    CHECK_EQ(frac.num, 2);
    CHECK_EQ(frac.den, 1);
    CHECK_EQ(asl_walk(ASL_ISAM_SEQ, 10, 3, 6, &walk), 0);
    CHECK_EQ(walk.cmp, 4);
    CHECK_EQ(asl_walk(ASL_ISAM_BIN, 10, 3, 6, &walk), 0);
    CHECK_EQ(walk.cmp, 3);
    CHECK_EQ(asl_walk(ASL_INDEX, 10, 0, 6, &walk), 0);
    CHECK_EQ(walk.cmp, 3);
    CHECK_EQ(asl_walk(ASL_SEQ, 10000, 0, 100, &walk), -2);
}

static void test_links(void) {
    Fs fs;
    FsOut out;

    fs_init(&fs, FS_SCHEME_INODE);
    CHECK_EQ(fs_create(&fs, "/F1", 1, &out), FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(fs_symlink(&fs, "/F2", "/F1", &out), FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(out.extra, 1);
    CHECK_EQ(fs_hard(&fs, "/F3", "/F1", &out), FS_OK);
    CHECK_EQ(out.ino, 1);
    CHECK_EQ(out.nlink, 2);
    CHECK_EQ(fs_unlink(&fs, "/F1", &out), FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(out.flag, FS_FCB_KEPT);
    CHECK_EQ(fs_lstat(&fs, "/F2", &out), FS_OK);
    CHECK_EQ(out.kind, FS_LNK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(fs_lstat(&fs, "/F3", &out), FS_OK);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(fs_stat(&fs, "/F2", &out), FS_DANGLE);
    CHECK_EQ(fs.node[0].alive, 1);
}

static void test_reads(void) {
    Fs fs;
    FsTrace tr;
    FsOut out;

    fs_init(&fs, FS_SCHEME_INODE);
    CHECK_EQ(fs_mkdir(&fs, "/stu", &out), FS_OK);
    CHECK_EQ(fs_mkdir(&fs, "/stu/course", &out), FS_OK);
    CHECK_EQ(fs_create(&fs, "/stu/course/course1", 1, &out), FS_OK);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_cd(&fs, "/stu/course", &tr, &out), FS_OK);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_cache(&fs, FS_CACHE_DATA, ".", 1, &out), FS_OK);
    CHECK_EQ(fs_load(&fs, "course1", &tr, &out), FS_OK);
    CHECK_EQ(out.reads, 2);

    fs_init(&fs, FS_SCHEME_INODE);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_create(&fs, "/a", 1, &out), FS_OK);
    CHECK_EQ(fs_mkdir(&fs, "/d", &out), FS_OK);
    CHECK_EQ(fs_create(&fs, "/d/b", 1, &out), FS_OK);
    CHECK_EQ(fs_open(&fs, 0, "/d/b", 1, &tr, &out), FS_OK);
    CHECK_EQ(out.reads, 4);
    fs_init(&fs, FS_SCHEME_FCB);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_mkdir(&fs, "/d", &out), FS_OK);
    CHECK_EQ(fs_create(&fs, "/d/b", 1, &out), FS_OK);
    CHECK_EQ(fs_open(&fs, 0, "/d/b", 1, &tr, &out), FS_OK);
    CHECK_EQ(out.reads, 2);
}

static void test_tables(void) {
    Fs fs;
    FsTrace tr;
    FsOut out;

    fs_init(&fs, FS_SCHEME_INODE);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_create(&fs, "/F", 1, &out), FS_OK);
    CHECK_EQ(fs_open(&fs, 0, "/F", 1, &tr, &out), FS_OK);
    memset(&tr, 0, sizeof tr);
    CHECK_EQ(fs_open(&fs, 1, "/F", 2, &tr, &out), FS_OK);
    CHECK_EQ(fs_open_count(&fs, out.ino), 2);
    CHECK_EQ(fs.sys[0].count, 2);
    CHECK_EQ(fs_pread(&fs, 0, 0, 0, &tr, &out), FS_OK);
    CHECK_EQ(fs.user[0][0].offset, 1);
    CHECK_EQ(fs.user[1][0].offset, 0);
    CHECK_EQ(fs_close(&fs, 0, 0, &out), FS_OK);
    CHECK_EQ(out.flag, 0);
    CHECK_EQ(out.nlink, 1);
    CHECK_EQ(fs_close(&fs, 1, 0, &out), FS_OK);
    CHECK_EQ(out.flag, 1);
    CHECK_EQ(out.extra, 0);
    CHECK_EQ(out.nlink, 1);
}

static void test_exam(void) {
    CHECK_EQ(exam_print("2009-31"), 0);
    CHECK_EQ(exam_print("2022-45"), 0);
    CHECK_EQ(exam_print("nope"), -1);
    CHECK_EQ(claim_print("close-nlink"), 0);
    CHECK_EQ(claim_print("missing"), -1);
}

int main(void) {
    test_asl();
    test_links();
    test_reads();
    test_tables();
    test_exam();
    return TEST_SUMMARY();
}
