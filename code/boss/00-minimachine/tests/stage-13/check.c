#include "fs/fs_vfs.h"
#include "kernel/k_file.h"
#include "test.h"
#include "user/user_boot.h"
#include "user/user_file.h"
#include "user/user_grep.h"
#include "user/user_net.h"
#include "user/user_scene.h"
#include "user/user_sh.h"
#include "user/user_sort.h"
#include "user/user_tool.h"
#include "user/user_zip.h"

#include <string.h>

static void expect_line(int (*fn)(const char *, char *, int), const char *id, const char *text) {
    char buf[640];
    CHECK(fn(id, buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, text);
}

static void test_shell(void) {
    UserCmd cmd[USER_NCMD];
    UserJob job;
    int n = 0;
    CHECK(user_sh_parse("echo hi > /d/a", cmd, USER_NCMD, &n) == 0);
    CHECK_EQ(n, 1);
    CHECK_EQ(cmd[0].argc, 2);
    CHECK_STR(cmd[0].arg[0], "echo");
    CHECK_STR(cmd[0].redir, "/d/a");
    CHECK(user_sh_parse("echo aba | grep aba > /out", cmd, USER_NCMD, &n) == 0);
    CHECK(user_sh_jobs(cmd, n, &job) == 0);
    CHECK_EQ(job.ncmd, 2);
    CHECK_EQ(job.pipes, 1);
    CHECK_EQ(job.redir, 1);
    CHECK_EQ(job.forks, 2);
    CHECK_EQ(job.execs, 2);
    CHECK_EQ(job.waits, 2);
    CHECK(user_sh_parse("echo a |", cmd, USER_NCMD, &n) != 0);
}

static void test_rt(void) {
    char buf[80];
    CHECK(user_rt_line(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "entry=00001000 sym=start write=1 exit=0");
}

static void test_grep(void) {
    char buf[160];
    expect_line(user_grep_line, "2015-8", "i=5 j=2 letter=C");
    expect_line(user_grep_line, "2019-9", "compares=10 pos=3 letter=B");
    CHECK(user_grep_text("abaabaabcabaabc", "abaabc", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "abaabaabcabaabc\nhost=grep kmp pos=3 compares=10");
}

static void test_sort(void) {
    static const int nine[] = {9, 30, 12, 18, 3, 17, 2, 6, 24};
    static const int six[] = {10, 35, 40, 50, 60, 200};
    long long wpl = 0;
    char buf[192];
    expect_line(user_sort_line, "2019-11", "dummy=2 letter=B");
    expect_line(user_sort_line, "2012-41", "cmp=825 io=1660");
    expect_line(user_sort_line, "practice-best", "wpl=223 io=446");
    expect_line(user_sort_line, "practice-io", "passes=4 io=3000000");
    expect_line(user_sort_line, "2016-11", "algo=merge letter=D");
    expect_line(user_sort_line, "2024-11", "seg=1 key=3 letter=D");
    CHECK(user_sort_wpl(nine, 9, 3, &wpl) == 0);
    CHECK_EQ(wpl, 223);
    CHECK(user_sort_wpl(six, 6, 2, &wpl) == 0);
    CHECK_EQ(wpl, 830);
    CHECK(user_sort_text(NULL, 3, "51 49 39 46 38 29 14 61 15 30 1", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "1 14 15 29 30 38 39 46 49 51 61\nhost=sort runs=3 first=4 passes=1 gen=22 io=22 files=0");
}

static void test_zip(void) {
    char buf[160];
    expect_line(user_zip_line, "2018-5", "a=00 b=1011 c=01 d=1010 e=11 f=100 wpl=80 letter=A");
    expect_line(user_zip_line, "2023-4", "wpl=90 avg=2.5 letter=B");
    expect_line(user_zip_line, "2014-6", "bad=110,1100 letter=D");
    expect_line(user_zip_line, "2019-3", "nodes=115 leaves=58 letter=C");
    CHECK(user_zip_text("aba", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "host=zip wpl=") == buf);
    CHECK(strstr(buf, "round=ok") != NULL);
}

static void test_files(void) {
    FsNs *ns = fs_ns_new();
    char buf[160];
    CHECK(ns != NULL);
    CHECK(user_mkdir(ns, "/d") == 0);
    CHECK(user_write(ns, "/d/a", "hi") == 0);
    CHECK(user_ln(ns, 0, "/d/a", "/d/b") == 0);
    CHECK(user_ls(ns, "/d", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "a b");
    CHECK(user_cat(ns, "/d/b", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "hi");
    CHECK(user_rm(ns, "/d/b") == 0);
    CHECK(user_cat(ns, "/d/a", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "hi");
    CHECK(user_df(ns, buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "blocks=23 inodes=29");
    CHECK(user_tool_run(ns, "echo abaabaabcabaabc | grep abaabc", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "abaabaabcabaabc\nhost=grep kmp pos=3 compares=10");
    CHECK(user_tool_run(ns, "echo 51 49 39 46 38 29 14 61 15 30 1 | sort 3", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "files=3") != NULL);
    CHECK(strstr(buf, "io=22") != NULL);
    CHECK(user_cat(ns, "/s/r0", buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "39 46 49 51");
    fs_ns_free(ns);
}

static void test_scene(void) {
    char buf[640];
    expect_line(user_scene_line, "cache", "va=03FFF180 vpn=3FFF off=180 pa=00153180 set=0 tag=2A63");
    expect_line(user_scene_line, "pv", "safe=yes count=66 order=P1 P2 P0 P3 P4");
    expect_line(user_scene_line, "fs", "algo=scan move=325");
    CHECK(user_scene_line("tcp", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "seq=11221") != NULL);
    CHECK(strstr(buf, "rtt=11") != NULL);
    CHECK(user_scene_line("all", buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "cache va=03FFF180") == buf);
    CHECK(strstr(buf, "move=325") != NULL);
}

static void test_boot(void) {
    char buf[240];
    CHECK(user_boot_line(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "magic=MMFS kernel=1 root=/ init=1 idle=0 shell=1");
    CHECK(user_visit_line(buf, (int)sizeof buf) == 0);
    CHECK(strstr(buf, "A=1 B=1 magic=MMFS\n") == buf);
    CHECK(strstr(buf, "hs seq=5000 ack=1001\n") != NULL);
    CHECK(strstr(buf, "arp=10.0.0.2 frame=1 dma=1 irq=2 rtt=2 syscall=19,20,21,22") != NULL);
    CHECK(user_ping_line(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "who=10.0.0.2 same=1");
    CHECK(user_curl_line(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "rtt=2 mode=nonpersist");
    CHECK(user_mail_tool(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "a=SMTP b=SMTP c=POP3 letter=D");
}

int main(void) {
    test_shell();
    test_rt();
    test_grep();
    test_sort();
    test_zip();
    test_files();
    test_scene();
    test_boot();
    return TEST_SUMMARY();
}
