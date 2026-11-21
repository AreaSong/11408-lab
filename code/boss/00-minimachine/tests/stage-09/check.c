#include "common/mm_machine.h"
#include "cpu/cpu_intr.h"
#include "dev/dev_alloc.h"
#include "dev/dev_arb.h"
#include "dev/dev_buf.h"
#include "dev/dev_bus.h"
#include "dev/dev_chan.h"
#include "dev/dev_exam.h"
#include "dev/dev_fmt.h"
#include "dev/dev_layer.h"
#include "dev/dev_port.h"
#include "dev/dev_ring.h"
#include "dev/dev_share.h"
#include "dev/dev_spool.h"
#include "dev/dev_wire.h"
#include "kernel/k_trap.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static void test_bw(void) {
    DevBw bw;
    double hz = 0;
    CHECK(dev_parse_hz(10, "MHz", &hz));
    CHECK(dev_bw(hz, 2, 1, 32, 1, 1, &bw) == 0);
    CHECK_STR(bw.brief, "20 MB/s");
    CHECK(dev_parse_hz(66, "MHz", &hz));
    CHECK(dev_bw(hz, 1, 2, 32, 1, 1, &bw) == 0);
    CHECK_STR(bw.brief, "528 MB/s");
    CHECK(dev_parse_hz(1333, "MHz", &hz));
    CHECK(dev_bw(hz, 1, 1, 64, 1, 3, &bw) == 0);
    CHECK_STR(bw.brief, "31.992 GB/s");
    CHECK(dev_parse_hz(2.4, "GHz", &hz));
    CHECK(dev_bw(hz, 1, 2, 16, 2, 1, &bw) == 0);
    CHECK_STR(bw.brief, "19.2 GB/s");
    CHECK(dev_parse_hz(420, "MHz", &hz));
    CHECK(dev_bw(hz, 1, 2, 64, 1, 1, &bw) == 0);
    CHECK_STR(bw.brief, "6.72 GB/s");
    CHECK(dev_parse_hz(1333, "MHz", &hz));
    CHECK(dev_bw(hz, 1, 1, 64, 1, 1, &bw) == 0);
    CHECK_STR(bw.brief, "10.664 GB/s");
}

static void test_txn(void) {
    DevTxn txn;
    double hz = 0;
    double prep = 0;
    CHECK(dev_parse_hz(100, "MHz", &hz));
    CHECK(dev_parse_seconds(0, "ns", &prep));
    CHECK(dev_txn(0, hz, 1, 1, 0, 4, prep, 32, 16, &txn) == 0);
    CHECK_STR(txn.time_brief, "50 ns");
    CHECK_STR(txn.avg_brief, "320 MB/s");
    CHECK_STR(txn.peak_brief, "400 MB/s");
    CHECK(dev_parse_hz(1, "GHz", &hz));
    CHECK(dev_parse_seconds(6, "ns", &prep));
    CHECK(dev_txn(1, hz, 4, 1, 0, 1, prep, 64, 32, &txn) == 0);
    CHECK_STR(txn.time_brief, "32 ns");
    CHECK_STR(txn.peak_brief, "8 GB/s");
    CHECK(dev_parse_hz(420, "MHz", &hz));
    CHECK(dev_parse_seconds(0, "ns", &prep));
    CHECK(dev_txn(0, hz, 1, 1, 2, 4, prep, 64, 64, &txn) == 0);
    CHECK_STR(txn.time_brief, "16.666667 ns");
    CHECK_STR(txn.avg_brief, "3.84 GB/s");
    CHECK_STR(txn.peak_brief, "6.72 GB/s");
}

static void test_share(void) {
    DevShare s;
    DevFrame frame;
    DevTask task;
    double hz = 0;
    double bps = 0;
    double gap = 0;
    CHECK(dev_parse_hz(50, "MHz", &hz));
    CHECK(dev_share_poll(hz, 500, 200, &s) == 0);
    CHECK_STR(s.pct, "0.2%");
    CHECK(dev_parse_hz(500, "MHz", &hz));
    CHECK(dev_parse_rate(2, "MB/s", &bps));
    CHECK(dev_share_query(hz, 4, 10, bps, 4, &s) == 0);
    CHECK_STR(s.pct, "4%");
    CHECK_STR(s.gap, "2 us");
    CHECK(dev_parse_rate(40, "MB/s", &bps));
    CHECK(dev_share_fit(hz, 400, bps, 4, &s) == 0);
    CHECK_EQ(s.keeps, 0);
    CHECK_STR(s.pct, "800%");
    CHECK(dev_share_dma(hz, 500, bps, 1000, &s) == 0);
    CHECK_STR(s.pct, "4%");
    CHECK(dev_parse_rate(0.5, "MB/s", &bps));
    CHECK(dev_share_intr(hz, 5, 18, 2, bps, 4, &s) == 0);
    CHECK_STR(s.pct, "2.5%");
    CHECK(dev_parse_rate(5, "MB/s", &bps));
    CHECK(dev_share_dma(hz, 500, bps, 5000, &s) == 0);
    CHECK_STR(s.pct, "0.1%");
    CHECK_EQ(dev_dma_requests(512, 64), 64);
    CHECK(dev_parse_seconds(0.5, "ms", &gap));
    CHECK(dev_serial(1, 7, 1, 1, gap, &frame) == 0);
    CHECK_STR(frame.bits_text, "10");
    CHECK_STR(frame.rate_text, "2000");
    CHECK(dev_parse_hz(50, "MHz", &hz));
    CHECK(dev_task(hz, gap, 4, 10, 20, 15, 1000, &task) == 0);
    CHECK_STR(task.total_text, "25070000");
    CHECK_STR(task.cpu_text, "90000");
}

static void test_buf(void) {
    long long single = dev_buf_closed(0, 10, 100, 50, 50);
    long long dual = dev_buf_closed(1, 10, 100, 50, 50);
    CHECK_EQ(single, 1550);
    CHECK_EQ(dual, 1100);
    CHECK_EQ(dev_buf_finish(0, 10, 100, 50, 50, NULL), single);
    CHECK_EQ(dev_buf_finish(1, 10, 100, 50, 50, NULL), dual);
    CHECK_EQ(dev_buf_closed(0, 2, 100, 5, 90), 300);
    CHECK_EQ(dev_buf_closed(1, 2, 100, 5, 90), 295);
}

static void test_alloc(void) {
    DevSys *s = dev_sys_new(NULL);
    CHECK(s != NULL);
    CHECK(dev_sys_add(s, "block", "d0", "c0", "ch0") == 0);
    CHECK(dev_sys_add(s, "block", "d1", "c0", "ch0") == 0);
    CHECK(dev_sys_proc(s, "A") == 0);
    CHECK(dev_sys_proc(s, "B") == 0);
    CHECK(dev_sys_proc(s, "C") == 0);
    CHECK(dev_sys_map(s, "A", "disk", "d0") == 0);
    CHECK(dev_sys_map(s, "B", "disk", "d1") == 0);
    CHECK(dev_sys_map(s, "C", "disk", "d0") == 0);
    CHECK(dev_sys_ask(s, "A", "disk") == 0);
    CHECK(dev_sys_ask(s, "B", "disk") == 0);
    CHECK(dev_sys_ask(s, "C", "disk") == 0);
    CHECK_STR(dev_sys_stage(s, "A"), "io");
    CHECK_STR(dev_sys_stage(s, "B"), "wait-ctrl");
    CHECK_STR(dev_sys_stage(s, "C"), "wait-dev");
    CHECK(dev_sys_done(s, "A") == 0);
    CHECK_STR(dev_sys_stage(s, "A"), "free");
    CHECK_STR(dev_sys_stage(s, "B"), "io");
    CHECK_STR(dev_sys_stage(s, "C"), "wait-ctrl");
    dev_sys_free(s);
}

static void test_buffers(void) {
    DevSpool *sp;
    DevRing *r = dev_ring_new(3, NULL);
    DevPool *p = dev_pool_new(2, NULL);
    int id;
    sp = dev_spool_new(3, NULL);
    CHECK(dev_spool_submit(sp, "A", "j1") == 0);
    CHECK(dev_spool_submit(sp, "B", "j2") == 0);
    CHECK(dev_spool_submit(sp, "C", "j3") == 0);
    CHECK(dev_spool_submit(sp, "D", "j4") == 0);
    CHECK_EQ(dev_spool_well(sp), 3);
    CHECK_EQ(dev_spool_waiting(sp), 1);
    CHECK(dev_spool_print(sp) == 0);
    CHECK(dev_spool_busy(sp));
    CHECK_STR(dev_spool_printing(sp), "j1");
    CHECK_STR(dev_spool_at(sp, 0), "j2");
    CHECK_EQ(dev_spool_waiting(sp), 0);
    dev_spool_free(sp);

    CHECK(dev_ring_put(r, "a") == 0);
    CHECK(dev_ring_put(r, "b") == 0);
    CHECK(dev_ring_put(r, "c") == 0);
    CHECK(dev_ring_put(r, "x") == 0);
    CHECK_EQ(dev_ring_count(r), 3);
    CHECK(dev_ring_get(r) == 0);
    CHECK_EQ(dev_ring_count(r), 2);
    dev_ring_free(r);
    id = dev_pool_get(p);
    CHECK_EQ(id, 0);
    CHECK(dev_pool_put(p, id) == 0);
    CHECK_EQ(dev_pool_free_count(p), 2);
    dev_pool_free(p);
}

static void test_words(void) {
    char lines[8][128];
    char layer[4][80];
    char result[32];
    char letter[4];
    char extra[64];
    char value[16];
    int req[] = {0, 0, 1};
    int winner = 0;
    int passed = 0;
    unsigned char mem[8];
    unsigned char devb[8] = {11, 22, 33, 44};
    DevChanInsn prog[2];
    int steps = 0;
    int irq = 0;
    CHECK(dev_wire("sync", lines, 8, result, (int)sizeof result) > 0);
    CHECK_STR(result, "统一时钟");
    CHECK(strstr(lines[1], "没有应答信号") != NULL);
    CHECK(dev_wire("full", lines, 8, result, (int)sizeof result) > 0);
    CHECK_STR(result, "全互锁");
    CHECK(dev_daisy(req, 3, &winner, &passed) == 0);
    CHECK_EQ(winner, 2);
    CHECK_EQ(passed, 2);
    CHECK(dev_layer_read("disk", "d0", layer, 4) == 4);
    CHECK(strstr(layer[0], "name=user") != NULL);
    CHECK(strstr(layer[1], "phys=d0") != NULL);
    CHECK(strstr(layer[2], "name=driver") != NULL);
    CHECK(strstr(layer[3], "name=interrupt") != NULL);
    CHECK(dev_iexam("2011-31", letter, 4, extra, 64) == 0);
    CHECK_STR(letter, "B");
    CHECK_STR(extra, "single=1550 double=1100");
    CHECK(dev_iexam("2013-27", letter, 4, extra, 64) == 0);
    CHECK_STR(letter, "C");
    CHECK(dev_iexam("2017-32", letter, 4, extra, 64) == 0);
    CHECK_STR(letter, "B");
    CHECK_STR(extra, "2,3,1,4");
    CHECK(dev_iexam("2024-22", letter, 4, extra, 64) == 0);
    CHECK_STR(letter, "C");
    CHECK(dev_iclaim("spool-user-moves", value, 16) == 0);
    CHECK_STR(value, "false");
    CHECK(dev_iclaim("dma-before-cpu", value, 16) == 0);
    CHECK_STR(value, "true");
    prog[0].op = DEV_CHAN_READ;
    prog[0].mem_at = 0;
    prog[0].dev_at = 0;
    prog[0].n = 4;
    prog[1].op = DEV_CHAN_HALT;
    prog[1].n = 0;
    CHECK(dev_chan_run(mem, 8, devb, 8, prog, 2, &steps, &irq) == 0);
    CHECK_EQ(steps, 2);
    CHECK_EQ(irq, 1);
    CHECK_EQ(mem[0], 11);
    CHECK_EQ(mem[3], 44);
}

static void test_machine(void) {
    unsigned char raw[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint32_t word = 0;
    uint32_t ch = 'Q';
    char text[16];
    MmMachine *m = mm_create(4096);
    CHECK(m != NULL);
    CHECK(dev_on(m) == 0);
    CHECK(dev_disk_fill(m, 0, raw, 8) == 0);
    CHECK(dev_dma_arm(m, 0, 0x100, 8, 1) == 0);
    CHECK(dev_after_mem(m) == 1);
    CHECK(dev_after_mem(m) == 1);
    CHECK_EQ(dev_dma_left(m), 0);
    CHECK_EQ(dev_dma_steals(m), 2);
    CHECK(cpu_irq_pending(m, 1));
    CHECK(mm_read(m, 0x100, 4, &word) == 0);
    CHECK_EQ(word, 0x44332211u);
    CHECK(mm_read(m, 0x104, 4, &word) == 0);
    CHECK_EQ(word, 0x88776655u);
    CHECK(dev_print_byte(m, 'A') == 0);
    CHECK(dev_print_byte(m, 'B') != 0);
    CHECK(dev_print_done(m) == 0);
    CHECK(cpu_irq_pending(m, 4));
    CHECK_EQ(dev_print_at(m, 0), 'A');
    CHECK(dev_mmio(m, DEV_CON_OUT, 1, &ch, 1) == 0);
    CHECK(k_console_out(m, text, (int)sizeof text) > 0);
    CHECK_STR(text, "Q");
    CHECK(mm_raw_read(m, DEV_CON_OUT, 1, &word) != 0 || word == 0);
    mm_destroy(m);
}

int main(void) {
    test_bw();
    test_txn();
    test_share();
    test_buf();
    test_alloc();
    test_buffers();
    test_words();
    test_machine();
    return TEST_SUMMARY();
}
