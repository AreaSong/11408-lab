#include "extra.h"
#include "fields.h"
#include "geom.h"
#include "map.h"
#include "series.h"
#include "test.h"
#include "trans.h"

#include <string.h>

static void test_2013(void) {
    PageGeom page;
    TlbGeom tlb;
    TlbSlot slots[4];
    TransIn in;
    TransOut out;
    memset(slots, 0, sizeof slots);
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 32, 28, 4096) == 0);
    CHECK_EQ(page.vpn_bits, 20);
    CHECK_EQ(page.offset_bits, 12);
    CHECK_EQ(page.pfn_bits, 16);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 4, 4) == 0);
    CHECK_EQ(tlb.tag_bits, 20);
    CHECK_EQ(tlb.index_bits, 0);
    slots[0].given = 1;
    slots[0].tag = 0xFF180;
    slots[0].frame = 0x0002;
    slots[1].given = 1;
    slots[1].valid = 1;
    slots[1].tag = 0x3FFF1;
    slots[1].frame = 0x0035;
    slots[2].given = 1;
    slots[2].tag = 0x02FF3;
    slots[2].frame = 0x0351;
    slots[3].given = 1;
    slots[3].valid = 1;
    slots[3].tag = 0x03FFF;
    slots[3].frame = 0x0153;
    in.page = &page;
    in.tlb = &tlb;
    in.slots = slots;
    in.va = 0x03FFF180;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_TLB);
    CHECK_EQ(out.vpn, 0x03FFF);
    CHECK_EQ(out.offset, 0x180);
    CHECK_EQ(out.frame, 0x0153);
    CHECK_EQ(out.pa, 0x0153180);
}

static void test_2022(void) {
    PageGeom page;
    PteItem ptes[3];
    TransIn in;
    TransOut out;
    uint64_t vpn = 0;
    uint64_t offset = 0;
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 32, 24, 4096) == 0);
    CHECK(va_split(&page, 0x00082840, &vpn, &offset) == 0);
    CHECK_EQ(vpn, 130);
    CHECK_EQ(offset, 0x840);
    ptes[0].vpn = 82;
    ptes[0].present = 0;
    ptes[0].frame = 0x024;
    ptes[1].vpn = 129;
    ptes[1].present = 1;
    ptes[1].frame = 0x180;
    ptes[2].vpn = 130;
    ptes[2].present = 1;
    ptes[2].frame = 0x018;
    in.page = &page;
    in.ptes = ptes;
    in.n_pte = 3;
    in.va = 0x00082840;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_PTE);
    CHECK_EQ(out.pa, 0x018840);
}

static void test_2011_cache(void) {
    PageGeom page;
    PteItem pte;
    Fields fields;
    LineSlot lines[8];
    TransIn in;
    TransOut out;
    memset(lines, 0, sizeof lines);
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 24, 20, 4096) == 0);
    CHECK_EQ(page.vpn_bits, 12);
    CHECK_EQ(page.pfn_bits, 8);
    CHECK(cache_fields(&fields, 20, 8, 32, 1) == 0);
    CHECK_EQ(fields.tag_bits, 12);
    CHECK_EQ(fields.index_bits, 3);
    CHECK_EQ(fields.offset_bits, 5);
    pte.vpn = 1;
    pte.present = 1;
    pte.frame = 0x04;
    lines[3].given = 1;
    lines[3].valid = 1;
    lines[3].tag = 0x105;
    in.page = &page;
    in.ptes = &pte;
    in.n_pte = 1;
    in.cache = &fields;
    in.lines = lines;
    in.va = 0x001C60;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.pa, 0x04C60);
    CHECK_EQ(out.cache_set, 3);
    CHECK_EQ(out.cache_tag, 0x04C);
    CHECK_EQ(out.cache_hit, 0);
}

static void test_2011_tlb(void) {
    PageGeom page;
    TlbGeom tlb;
    TlbSlot slots[8];
    TransIn in;
    TransOut out;
    memset(slots, 0, sizeof slots);
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 24, 20, 4096) == 0);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 8, 4) == 0);
    CHECK_EQ(tlb.n_sets, 2);
    CHECK_EQ(tlb.index_bits, 1);
    CHECK_EQ(tlb.tag_bits, 11);
    slots[3].given = 1;
    slots[3].valid = 1;
    slots[3].tag = 0x012;
    slots[3].frame = 0x1F;
    in.page = &page;
    in.tlb = &tlb;
    in.slots = slots;
    in.va = 0x024BAC;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_TLB);
    CHECK_EQ(out.tlb_set, 0);
    CHECK_EQ(out.tlb_tag, 0x012);
    CHECK_EQ(out.frame, 0x1F);
    CHECK_EQ(out.pa, 0x1FBAC);
}

static void test_2018(void) {
    PageGeom page;
    TlbGeom tlb;
    TlbSlot slots[4];
    Fields fields;
    LineSlot lines[32];
    TransIn in;
    TransOut out;
    uint64_t block = 0;
    uint64_t tag = 0;
    uint64_t offset = 0;
    int set = 0;
    memset(slots, 0, sizeof slots);
    memset(lines, 0, sizeof lines);
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 32, 28, 4096) == 0);
    CHECK_EQ(page.pfn_bits, 16);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 4, 4) == 0);
    slots[0].given = 1;
    slots[0].valid = 1;
    slots[0].tag = 0x008C;
    slots[0].frame = 0x0040;
    CHECK(cache_fields(&fields, 28, 16, 32, 2) == 0);
    CHECK_EQ(fields.tag_bits, 20);
    CHECK_EQ(fields.index_bits, 3);
    CHECK_EQ(fields.offset_bits, 5);
    lines[2 * 2 + 0].given = 1;
    lines[2 * 2 + 0].valid = 0;
    lines[2 * 2 + 0].tag = 0x00400;
    lines[2 * 2 + 1].given = 1;
    lines[2 * 2 + 1].valid = 1;
    lines[2 * 2 + 1].tag = 0;
    in.page = &page;
    in.tlb = &tlb;
    in.slots = slots;
    in.cache = &fields;
    in.lines = lines;
    in.va = 0x0008C040;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_TLB);
    CHECK_EQ(out.pa, 0x0040040);
    CHECK_EQ(out.cache_set, 2);
    CHECK_EQ(out.cache_tag, 0x400);
    CHECK_EQ(out.cache_hit, 0);
    CHECK_EQ(out.invalid_tag, 1);
    CHECK(fields_locate(&fields, 0x0007C260, &block, &set, &tag, &offset) == 0);
    CHECK_EQ(set, 3);
}

static void test_2021(void) {
    PageGeom page;
    TlbGeom tlb30;
    TlbGeom tlb32;
    uint64_t vpns[] = {10, 12, 16, 7, 26, 4, 12, 20};
    uint64_t victims[8];
    int n_victims = 0;
    int hits = 0;
    CHECK(page_geom(&page, 30, 24, 4096) == 0);
    CHECK_EQ(page.vpn_bits, 18);
    CHECK_EQ(page.offset_bits, 12);
    CHECK_EQ(page.pfn_bits, 12);
    CHECK(tlb_geom(&tlb30, page.vpn_bits, 16, 2) == 0);
    CHECK_EQ(tlb30.n_sets, 8);
    CHECK_EQ(tlb30.index_bits, 3);
    CHECK_EQ(tlb30.tag_bits, 15);
    CHECK(page_geom(&page, 32, 24, 4096) == 0);
    CHECK(tlb_geom(&tlb32, page.vpn_bits, 16, 2) == 0);
    CHECK_EQ(tlb32.tag_bits, 17);
    CHECK_EQ(tlb32.tag_bits - tlb30.tag_bits, 2);
    CHECK(series_victims(18, 16, 2, vpns, 8, victims, &n_victims, &hits) == 0);
    CHECK_EQ(hits, 1);
    CHECK_EQ(n_victims, 1);
    CHECK_EQ(victims[0], 4);
}

static void test_2016(void) {
    PageGeom page;
    TlbGeom tlb;
    Fields fields;
    int set = 0;
    uint64_t tag = 0;
    CHECK(page_geom(&page, 32, 24, 8192) == 0);
    CHECK_EQ(page.vpn_bits, 19);
    CHECK_EQ(page.offset_bits, 13);
    CHECK_EQ(page.pfn_bits, 11);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 4, 4) == 0);
    CHECK_EQ(tlb.tag_bits, 19);
    CHECK_EQ(tlb.index_bits, 0);
    CHECK(cache_from_bytes(&fields, 24, 65536, 64, 2) == 0);
    CHECK_EQ(fields.n_lines, 1024);
    CHECK_EQ(fields.n_sets, 512);
    CHECK_EQ(fields.tag_bits, 9);
    CHECK_EQ(fields.index_bits, 9);
    CHECK_EQ(fields.offset_bits, 6);
    CHECK(block_place(&fields, 4099, &set, &tag) == 0);
    CHECK_EQ(set, 3);
    CHECK_EQ(tag, 8);
}

static void test_2024_tlb(void) {
    PageGeom page;
    TlbGeom tlb;
    CHECK(page_geom(&page, 32, 30, 1024) == 0);
    CHECK_EQ(page.vpn_bits, 22);
    CHECK_EQ(page.pfn_bits, 20);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 32, 4) == 0);
    CHECK_EQ(tlb.n_sets, 8);
    CHECK_EQ(tlb.index_bits, 3);
    CHECK_EQ(tlb.tag_bits, 19);
}

static void test_2024_pte(void) {
    PteLoc loc;
    CHECK(pte_locate(32, 4194304, 4, 0xB8C00000, 0x65400000, 0x12345678, 0xBAB45678, &loc) == 0);
    CHECK_EQ(loc.vpn, 0x48);
    CHECK_EQ(loc.offset, 0x345678);
    CHECK_EQ(loc.pte_va, 0xB8C00120);
    CHECK_EQ(loc.pte_pa, 0x65400120);
    CHECK_EQ(loc.new_frame, 0x2EA);
    CHECK_EQ(loc.pt_vpn, 0x2E3);
    CHECK_EQ(loc.pt_pte_va, 0xB8C00B8C);
    CHECK_EQ(loc.pt_frame, 0x195);
}

static void test_invalid_and_fault(void) {
    PageGeom page;
    TlbGeom tlb;
    TlbSlot slots[2];
    PteItem pte;
    TransIn in;
    TransOut out;
    int fault = 0;
    uint64_t pa = 0;
    int kind = 0;
    memset(slots, 0, sizeof slots);
    memset(&in, 0, sizeof in);
    CHECK(page_geom(&page, 32, 24, 4096) == 0);
    CHECK(tlb_geom(&tlb, page.vpn_bits, 2, 2) == 0);
    slots[0].given = 1;
    slots[0].valid = 0;
    slots[0].tag = 1;
    slots[0].frame = 0x9;
    pte.vpn = 1;
    pte.present = 1;
    pte.frame = 0x3;
    in.page = &page;
    in.tlb = &tlb;
    in.slots = slots;
    in.ptes = &pte;
    in.n_pte = 1;
    in.va = 0x00001010;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_PTE);
    CHECK_EQ(out.pa, 0x3010);
    pte.vpn = 3;
    pte.present = 0;
    in.tlb = NULL;
    in.va = 0x00003020;
    CHECK(trans_run(&in, &out) == 0);
    CHECK_EQ(out.via, VIA_FAULT);
    CHECK(seg_translate(0x8000, 0x100, 0x20, &fault, &pa) == 0);
    CHECK_EQ(fault, 0);
    CHECK_EQ(pa, 0x8020);
    CHECK(seg_translate(0x8000, 0x100, 0x100, &fault, &pa) == 0);
    CHECK_EQ(fault, 1);
    CHECK(segpage_translate(4096, 4, 1, 0x10, 1, 0xA, &kind, &pa) == 0);
    CHECK_EQ(kind, SEG_OK);
    CHECK_EQ(pa, 0xA010);
    CHECK(segpage_translate(4096, 4, 4, 0x10, 1, 0xA, &kind, &pa) == 0);
    CHECK_EQ(kind, SEG_OVER);
    CHECK(segpage_translate(4096, 4, 2, 0x10, 0, 0, &kind, &pa) == 0);
    CHECK_EQ(kind, SEG_FAULT);
}

int main(void) {
    test_2013();
    test_2022();
    test_2011_cache();
    test_2011_tlb();
    test_2018();
    test_2021();
    test_2016();
    test_2024_tlb();
    test_2024_pte();
    test_invalid_and_fault();
    return TEST_SUMMARY();
}
