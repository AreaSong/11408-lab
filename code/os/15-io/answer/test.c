#include "alloc.h"
#include "buf.h"
#include "ring.h"
#include "spool.h"
#include "test.h"

#include <string.h>

static void check_closed(int dual, int n, long long t, long long m, long long c) {
    CHECK_EQ(buf_finish(dual, n, t, m, c, 0), buf_closed(dual, n, t, m, c));
}

static void test_buf(void) {
    static const long long ts[] = {0, 1, 50, 100, 200};
    static const long long ms[] = {0, 5, 50};
    static const long long cs[] = {0, 50, 90, 200};
    static const int ns[] = {1, 2, 3, 10};
    BufStep step[2];
    int a, b, c, n, dual;

    CHECK_EQ(buf_closed(0, 10, 100, 50, 50), 1550);
    CHECK_EQ(buf_closed(1, 10, 100, 50, 50), 1100);
    CHECK_EQ(buf_closed(0, 2, 100, 5, 90), 300);
    CHECK_EQ(buf_closed(1, 2, 100, 5, 90), 295);
    CHECK_EQ(buf_closed(0, 4, 100, 50, 200), 1100);
    CHECK_EQ(buf_closed(1, 4, 100, 50, 200), 1100);
    CHECK_EQ(buf_finish(0, 2, 10, 4, 6, step), 34);
    CHECK_EQ(step[0].in0, 0);
    CHECK_EQ(step[0].mv0, 10);
    CHECK_EQ(step[0].cpu1, 20);
    CHECK_EQ(step[1].in0, 14);
    CHECK_EQ(step[1].mv0, 24);
    CHECK_EQ(step[1].cpu1, 34);
    CHECK_EQ(buf_finish(1, 2, 10, 4, 6, step), 30);
    CHECK_EQ(step[1].buf, 1);
    CHECK_EQ(step[1].in0, 10);
    CHECK_EQ(step[1].cpu1, 30);
    for (dual = 0; dual < 2; dual++) {
        for (n = 0; n < 4; n++) {
            for (a = 0; a < 5; a++) {
                for (b = 0; b < 3; b++) {
                    for (c = 0; c < 4; c++) {
                        check_closed(dual, ns[n], ts[a], ms[b], cs[c]);
                    }
                }
            }
        }
    }
}

static int proc_of(const IoSys *s, const char *name) {
    int i;

    for (i = 0; i < s->nproc; i++) {
        if (strcmp(s->proc[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void test_fcfs(void) {
    IoSys s;
    int b;
    int c;

    memset(&s, 0, sizeof s);
    s.quiet = 1;
    io_init(&s);
    io_add(&s, "char", "p0", "c0", "ch0");
    io_proc(&s, "A");
    io_proc(&s, "B");
    io_proc(&s, "C");
    io_map(&s, "A", "LP", "p0");
    io_map(&s, "B", "LP", "p0");
    io_map(&s, "C", "LP", "p0");
    io_ask(&s, "A", "LP");
    io_ask(&s, "B", "LP");
    io_ask(&s, "C", "LP");
    io_done(&s, "A");
    b = proc_of(&s, "B");
    c = proc_of(&s, "C");
    CHECK_EQ(s.proc[b].stage, ST_IO);
    CHECK_EQ(s.proc[c].stage, ST_WAIT_DEV);
    CHECK_EQ(s.dev[0].owner, b);
    CHECK_EQ(s.dev[0].q.n, 1);
    CHECK_EQ(s.dev[0].q.id[0], c);
    CHECK_EQ(s.proc[proc_of(&s, "A")].stage, ST_FREE);
}

static void test_share(void) {
    IoSys s;
    int b;
    int c;

    memset(&s, 0, sizeof s);
    s.quiet = 1;
    io_init(&s);
    io_add(&s, "block", "d0", "c0", "ch0");
    io_add(&s, "block", "d1", "c0", "ch0");
    io_proc(&s, "A");
    io_proc(&s, "B");
    io_proc(&s, "C");
    io_map(&s, "A", "disk", "d0");
    io_map(&s, "B", "disk", "d1");
    io_map(&s, "C", "disk", "d0");
    io_ask(&s, "A", "disk");
    io_ask(&s, "B", "disk");
    io_ask(&s, "C", "disk");
    CHECK_EQ(s.proc[proc_of(&s, "B")].stage, ST_WAIT_CTRL);
    CHECK_EQ(s.dev[1].owner, proc_of(&s, "B"));
    CHECK_EQ(s.proc[proc_of(&s, "C")].stage, ST_WAIT_DEV);
    io_done(&s, "A");
    b = proc_of(&s, "B");
    c = proc_of(&s, "C");
    CHECK_EQ(s.proc[b].stage, ST_IO);
    CHECK_EQ(s.ctrl[0].owner, b);
    CHECK_EQ(s.chan[0].owner, b);
    CHECK_EQ(s.proc[c].stage, ST_WAIT_CTRL);
    CHECK_EQ(s.dev[0].owner, c);
    CHECK_EQ(s.ctrl[0].q.n, 1);
    CHECK_EQ(s.ctrl[0].q.id[0], c);
}

static void test_channel(void) {
    IoSys s;
    int b;

    memset(&s, 0, sizeof s);
    s.quiet = 1;
    io_init(&s);
    io_add(&s, "block", "d0", "c0", "ch0");
    io_add(&s, "block", "d1", "c1", "ch0");
    io_proc(&s, "A");
    io_proc(&s, "B");
    io_map(&s, "A", "disk", "d0");
    io_map(&s, "B", "disk", "d1");
    io_ask(&s, "A", "disk");
    io_ask(&s, "B", "disk");
    b = proc_of(&s, "B");
    CHECK_EQ(s.proc[b].stage, ST_WAIT_CHAN);
    CHECK_EQ(s.ctrl[1].owner, b);
    io_done(&s, "A");
    CHECK_EQ(s.proc[b].stage, ST_IO);
    CHECK_EQ(s.chan[0].owner, b);
    CHECK_EQ(s.ctrl[0].owner, -1);
}

static void test_logic(void) {
    IoSys s;

    memset(&s, 0, sizeof s);
    s.quiet = 1;
    io_init(&s);
    io_add(&s, "char", "p0", "c0", "ch0");
    io_add(&s, "char", "p1", "c1", "ch1");
    io_proc(&s, "A");
    io_proc(&s, "B");
    io_map(&s, "A", "LP", "p0");
    io_map(&s, "B", "LP", "p1");
    io_ask(&s, "A", "LP");
    io_ask(&s, "B", "LP");
    CHECK_EQ(s.proc[0].stage, ST_IO);
    CHECK_EQ(s.proc[1].stage, ST_IO);
    CHECK_EQ(s.proc[0].dev, 0);
    CHECK_EQ(s.proc[1].dev, 1);
    io_ask(&s, "A", "LP");
    CHECK_EQ(s.proc[0].stage, ST_IO);
}

static void test_ring(void) {
    Ring r;

    memset(&r, 0, sizeof r);
    r.quiet = 1;
    ring_new(&r, 3);
    ring_put(&r, "a");
    ring_put(&r, "b");
    ring_put(&r, "c");
    ring_put(&r, "x");
    CHECK_EQ(r.n, 3);
    ring_get(&r);
    ring_put(&r, "d");
    CHECK_EQ(r.n, 3);
    CHECK_STR(r.data[(r.head + 0) % r.cap], "b");
    CHECK_STR(r.data[(r.head + 1) % r.cap], "c");
    CHECK_STR(r.data[(r.head + 2) % r.cap], "d");
    ring_get(&r);
    ring_get(&r);
    ring_get(&r);
    ring_get(&r);
    CHECK_EQ(r.n, 0);
}

static void test_spool(void) {
    Spool s;

    memset(&s, 0, sizeof s);
    s.quiet = 1;
    spool_init(&s, 3);
    spool_submit(&s, "A", "j1");
    spool_submit(&s, "B", "j2");
    spool_submit(&s, "C", "j3");
    spool_submit(&s, "D", "j4");
    CHECK_EQ(s.n, 3);
    CHECK_EQ(s.wn, 1);
    CHECK_EQ(s.busy, 0);
    spool_print(&s);
    CHECK_EQ(s.busy, 1);
    CHECK_STR(s.cur.job, "j1");
    CHECK_STR(s.cur.proc, "A");
    CHECK_EQ(s.n, 3);
    CHECK_EQ(s.wn, 0);
    CHECK_STR(s.well[(s.head + s.n - 1) % s.cap].job, "j4");
    spool_print(&s);
    CHECK_EQ(s.busy, 1);
    CHECK_STR(s.cur.job, "j1");
    spool_finish(&s);
    CHECK_EQ(s.busy, 0);
    spool_print(&s);
    CHECK_STR(s.cur.job, "j2");
}

int main(void) {
    test_buf();
    test_fcfs();
    test_share();
    test_channel();
    test_logic();
    test_ring();
    test_spool();
    return TEST_SUMMARY();
}
