#include "addr.h"
#include "eat.h"
#include "exam.h"
#include "geom.h"
#include "seg.h"
#include "test.h"
#include "trace.h"
#include "trans.h"

static void test_geom(void) {
    Geom g;

    CHECK_EQ(geom_bits(32, 4096, 4, &g), 0);
    CHECK_EQ(g.offset_bits, 12);
    CHECK_EQ(g.page_bits, 20);
    CHECK_EQ(g.pages, 1048576);
    CHECK_EQ(g.table_bytes, 4194304);
    CHECK_EQ(g.levels, 2);
    CHECK_EQ(g.top_entries, 1024);
    CHECK_EQ(g.tables_pages, 1025);
    CHECK_EQ(g.fits, 1);

    CHECK_EQ(geom_pages(65536, 1024, 2, &g), 0);
    CHECK_EQ(g.va_bits, 26);
    CHECK_EQ(g.top_entries, 128);
    CHECK_EQ(g.level_bits, 9);

    CHECK_EQ(geom_bits(32, 4096, 8, &g), 0);
    CHECK_EQ(g.levels, 3);
    CHECK_EQ(g.top_bits, 2);
    CHECK_EQ(g.top_bytes, 32);
    CHECK_EQ(g.terms[1], 4);
    CHECK_EQ(g.terms[2], 2048);
    CHECK_EQ(g.tables_pages, 2053);
    CHECK_EQ(geom_bits(32, 1000, 4, &g), -1);
}

static void test_2013_and_2018(void) {
    int widths[] = {10, 10, 12};
    unsigned long long values[] = {6, 6, 8};
    unsigned long long addr = 0;
    unsigned long long parts[3];
    unsigned long long page = 0;
    unsigned long long pte = 0;
    unsigned long long frame = 0;
    unsigned long long pa = 0;
    int count = 0;
    int page_widths[] = {20, 12};

    CHECK_EQ(addr_pack(widths, values, 3, &addr), 0);
    CHECK_EQ(addr, 0x1806008ULL);
    CHECK_EQ(addr_cut(widths, 3, addr, parts), 0);
    CHECK_EQ(parts[0], 6);
    CHECK_EQ(parts[1], 6);
    CHECK_EQ(parts[2], 8);

    CHECK_EQ(span_count(4096, 8192, &count), 0);
    CHECK_EQ(count, 2);
    CHECK_EQ(span_at(4096, 4, 0x8000, 0x900000, 0x200000, 0, &page, &pte, &frame, &pa), 0);
    CHECK_EQ(page, 8);
    CHECK_EQ(pte, 0x200020ULL);
    CHECK_EQ(frame, 0x900ULL);
    CHECK_EQ(pa, 0x900000ULL);
    CHECK_EQ(span_at(4096, 4, 0x8000, 0x900000, 0x200000, 1, &page, &pte, &frame, &pa), 0);
    CHECK_EQ(pte, 0x200024ULL);
    CHECK_EQ(frame, 0x901ULL);
    CHECK_EQ(pa, 0x901000ULL);
    CHECK_EQ(addr_cut(page_widths, 2, 0x8000, parts), 0);
    CHECK_EQ(parts[0], 8);
    CHECK_EQ(parts[1], 0);
}

static void test_2020_and_2015(void) {
    unsigned long long index = 0;
    unsigned long long addr = 0;
    unsigned long long parts[3];
    unsigned long long other[3];
    unsigned long long pde = 0;
    unsigned long long base = 0;
    unsigned long long pte = 0;
    int widths[] = {10, 10, 12};

    CHECK_EQ(addr_elem(0x10800000ULL, 1024, 4, 1, 2, &index, &addr), 0);
    CHECK_EQ(index, 1026);
    CHECK_EQ(addr, 0x10801008ULL);
    CHECK_EQ(addr_cut(widths, 3, addr, parts), 0);
    CHECK_EQ(parts[0], 0x42ULL);
    CHECK_EQ(parts[1], 1);
    CHECK_EQ(parts[2], 8);
    CHECK_EQ(addr_slot(4, 0x00201000ULL, 0x42, &pde), 0);
    CHECK_EQ(pde, 0x00201108ULL);
    CHECK_EQ(addr_join(4096, 0x301, 0, &base), 0);
    CHECK_EQ(base, 0x301000ULL);
    CHECK_EQ(addr_slot(4, base, 1, &pte), 0);
    CHECK_EQ(pte, 0x301004ULL);

    CHECK_EQ(addr_cut(widths, 3, 0x01000000ULL, parts), 0);
    CHECK_EQ(addr_cut(widths, 3, 0x01112048ULL, other), 0);
    CHECK_EQ(parts[0], 4);
    CHECK_EQ(other[0], 4);
}

static void test_eat_and_trace(void) {
    unsigned long long num = 0;
    unsigned long long den = 0;
    unsigned long long ns = 0;
    Trace trace;
    GoInfo go;

    CHECK_EQ(eat_avg(9, 10, 20, 100, 1, &num, &den), 0);
    CHECK_EQ(num, 130);
    CHECK_EQ(den, 1);
    CHECK_EQ(eat_avg(9, 10, 20, 100, 2, &num, &den), 0);
    CHECK_EQ(num, 140);
    CHECK_EQ(eat_avg(1, 3, 10, 100, 1, &num, &den), 0);
    CHECK_EQ(num, 530);
    CHECK_EQ(den, 3);
    CHECK_EQ(eat_fault(10, 100, 1, 100000000ULL, &ns), 0);
    CHECK_EQ(ns, 100000220ULL);
    CHECK_EQ(eat_pt_hit(10, 100, 1, &ns), 0);
    CHECK_EQ(ns, 210);
    CHECK_EQ(eat_tlb_hit(10, 100, &ns), 0);
    CHECK_EQ(ns, 110);

    CHECK_EQ(trace_init(&trace, 4096, 10, 100, 100000000ULL, 2), 0);
    CHECK_EQ(trace_map(&trace, 0, 0x101, 1), 0);
    CHECK_EQ(trace_map(&trace, 1, 0, 0), 0);
    CHECK_EQ(trace_map(&trace, 2, 0x254, 1), 0);
    CHECK_EQ(trace_go(&trace, 0x2362, &go), 0);
    CHECK_EQ(go.result, GO_PT);
    CHECK_EQ(go.ns, 210);
    CHECK_EQ(go.pa, 0x254362ULL);
    CHECK_EQ(trace_go(&trace, 0x1565, &go), 0);
    CHECK_EQ(go.result, GO_FAULT);
    CHECK_EQ(go.victim, 0);
    CHECK_EQ(go.ns, 100000220ULL);
    CHECK_EQ(go.pa, 0x101565ULL);
    CHECK_EQ(trace_go(&trace, 0x25a5, &go), 0);
    CHECK_EQ(go.result, GO_TLB);
    CHECK_EQ(go.ns, 110);
    CHECK_EQ(go.pa, 0x2545a5ULL);
}

static void test_tlb_capacity(void) {
    Trace trace;
    GoInfo go;
    int i;

    /* 快表只有 8 项。第 9 个新页会挤掉最久没再访问的第 0 页。 */
    CHECK_EQ(trace_init(&trace, 4096, 1, 10, 1000, 16), 0);
    for (i = 0; i < 9; i++) {
        CHECK_EQ(trace_map(&trace, (unsigned long long)i, (unsigned long long)i, 1), 0);
        CHECK_EQ(trace_go(&trace, (unsigned long long)i * 4096, &go), 0);
        CHECK_EQ(go.result, GO_PT);
    }
    CHECK_EQ(trace_go(&trace, 0, &go), 0);
    CHECK_EQ(go.result, GO_PT);
    CHECK_EQ(trace_go(&trace, 8 * 4096, &go), 0);
    CHECK_EQ(go.result, GO_TLB);
}

static void test_segment(void) {
    int result = 0;
    unsigned long long pa = 0;
    unsigned long long segments = 0;
    unsigned long long max_len = 0;
    SegPageInfo segpage;

    CHECK_EQ(seg_len(32, 8, &segments, &max_len), 0);
    CHECK_EQ(segments, 256);
    CHECK_EQ(max_len, 16777216ULL);
    CHECK_EQ(seg_translate(0x4000, 1000, 999, &result, &pa), 0);
    CHECK_EQ(result, SEG_OK);
    CHECK_EQ(pa, 0x43e7ULL);
    CHECK_EQ(seg_translate(0x4000, 1000, 1000, &result, &pa), 0);
    CHECK_EQ(result, SEG_BOUND);
    CHECK_EQ(seg_check(4, 4, 100, 0, 0, &result, &pa), 0);
    CHECK_EQ(result, SEG_BOUND_NO);
    CHECK_EQ(seg_check(4, 1, 100, 100, 0, &result, &pa), 0);
    CHECK_EQ(result, SEG_BOUND_OFF);
    CHECK_EQ(seg_page(4096, 2, 1, 8, 1, 5, 0, &segpage), 0);
    CHECK_EQ(segpage.result, SP_PT);
    CHECK_EQ(segpage.visits, 3);
    CHECK_EQ(segpage.pa, 20488);
    CHECK_EQ(seg_page(4096, 2, 2, 0, 1, 5, 0, &segpage), 0);
    CHECK_EQ(segpage.result, SP_BOUND);
    CHECK_EQ(segpage.visits, 1);
    CHECK_EQ(seg_page(4096, 2, 1, 8, 0, 5, 0, &segpage), 0);
    CHECK_EQ(segpage.result, SP_FAULT);
    CHECK_EQ(seg_page(4096, 2, 1, 8, 1, 5, 1, &segpage), 0);
    CHECK_EQ(segpage.result, SP_TLB);
    CHECK_EQ(segpage.visits, 1);
    CHECK_EQ(seg_page(4096, 2, 2, 0, 1, 5, 1, &segpage), -2);
}

static void test_look_flat_exam(void) {
    unsigned long long pages = 0;
    unsigned long long internal = 0;
    LookInfo look;
    FlatInfo flat;
    char letter = '?';

    CHECK_EQ(trans_look(4096, 2, 0x180, 0, 2, 0x10, 1, 0x20, &look), 0);
    CHECK_EQ(look.result, LOOK_PT);
    CHECK_EQ(look.pa, 0x20180ULL);
    CHECK_EQ(trans_look(4096, 2, 0x180, 1, 2, 0x10, 1, 0x99, &look), 0);
    CHECK_EQ(look.result, LOOK_TLB);
    CHECK_EQ(look.frame, 0x10ULL);
    CHECK_EQ(look.pa, 0x10180ULL);
    CHECK_EQ(trans_look(4096, 2, 0x180, 1, 3, 0x10, 0, 0, &look), 0);
    CHECK_EQ(look.result, LOOK_FAULT);
    CHECK_EQ(trans_flat(4096, 4, 10, 0x200000, 10, 1, 1, 0, &flat), 0);
    CHECK_EQ(flat.result, FLAT_BOUND);
    CHECK_EQ(trans_flat(4096, 4, 10, 0x200000, 8, 0, 1, 0, &flat), 0);
    CHECK_EQ(flat.result, FLAT_FAULT);
    CHECK_EQ(flat.pte, 0x200020ULL);
    CHECK_EQ(addr_frag(4096, 5000, &pages, &internal), 0);
    CHECK_EQ(pages, 2);
    CHECK_EQ(internal, 3192);
    CHECK_EQ(exam_letter("2009-27", &letter), 0);
    CHECK_EQ(letter, 'C');
    CHECK_EQ(exam_letter("2010-29", &letter), 0);
    CHECK_EQ(letter, 'B');
    CHECK_EQ(exam_letter("2020-28", &letter), 0);
    CHECK_EQ(letter, 'D');
}

int main(void) {
    test_geom();
    test_2013_and_2018();
    test_2020_and_2015();
    test_eat_and_trace();
    test_tlb_capacity();
    test_segment();
    test_look_flat_exam();
    TEST_SUMMARY();
}
