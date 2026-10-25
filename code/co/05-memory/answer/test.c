#include <math.h>
#include <stdint.h>

#include "cross.h"
#include "disk.h"
#include "dram.h"
#include "expand.h"
#include "test.h"
#include "unit.h"

static void test_units(void) {
    uint64_t bytes = 0;
    double rate = 0;
    double dec = 0;
    double hz = 0;
    CHECK(parse_bin(4, "M", &bytes));
    CHECK_EQ(bytes, 4ull << 20);
    CHECK(parse_bin(4, "KB", &bytes));
    CHECK_EQ(bytes, 4096);
    CHECK(parse_dec_bytes(4, "KB", &dec));
    CHECK_EQ(llround(dec), 4000);
    CHECK(parse_rate(20, "MB/s", &rate));
    CHECK_EQ(llround(rate), 20000000);
    CHECK(parse_hz(1333, "MHz", &hz));
    CHECK_EQ(llround(hz / 1e6), 1333);
    CHECK(parse_addr("0B1FH", &bytes));
    CHECK_EQ(bytes, 0xB1F);
    CHECK(parse_addr("804001AH", &bytes));
    CHECK_EQ(bytes, 0x804001A);
}

static void test_dram_chips(void) {
    DramLayout best;
    DramLayout other;
    int has_other = 0;
    CHECK(dram_layout(4ull << 20, &best, &other, &has_other) == 0);
    CHECK_EQ(best.rows, 2048);
    CHECK_EQ(best.cols, 2048);
    CHECK_EQ(best.addr_pins, 11);
    CHECK(!has_other);
    CHECK(dram_layout(2048, &best, &other, &has_other) == 0);
    CHECK(has_other);
    CHECK_EQ(best.rows, 32);
    CHECK_EQ(best.cols, 64);
    CHECK_EQ(best.addr_pins, 6);
    CHECK_EQ(other.rows, 64);
    CHECK_EQ(other.cols, 32);
}

static void test_refresh(void) {
    RefreshPlan plan;
    CHECK(refresh_plan(128, 0.002, 0.5e-6, &plan) == 0);
    CHECK_EQ(plan.slots, 4000);
    CHECK_EQ(plan.rw_slots, 3872);
    CHECK_EQ(llround(plan.dead_s * 1e6), 64);
    CHECK_EQ(llround(plan.dead_ratio * 1000), 32);
    CHECK_EQ(llround(plan.sys_cycle_s * 1e6), 1);
    CHECK_EQ(llround(plan.sweep_s * 1e6), 128);
    CHECK_EQ(llround(plan.async_gap_s * 1e9), 15625);
}

static void test_expand_and_locate(void) {
    ExpandInfo info;
    LocateInfo loc;
    CHECK(expand_info(4 * 1024, 8, 2 * 1024, 8, &info) == 0);
    CHECK_EQ(info.chips, 2);
    CHECK_EQ(info.mode, 1);
    CHECK(expand_info(60 * 1024, 8, 4 * 1024, 4, &info) == 0);
    CHECK_EQ(info.chips, 30);
    CHECK_EQ(info.groups, 15);
    CHECK_EQ(info.per_group, 2);
    CHECK_EQ(info.mode, 2);
    CHECK_EQ(info.select_bits, 4);
    CHECK(expand_info(8 * 1024, 8, 2 * 1024, 4, &info) == 0);
    CHECK_EQ(info.chips, 8);
    CHECK(locate_info(8 * 1024, 2 * 1024, 0xB1F, &loc) == 0);
    CHECK_EQ(loc.group, 1);
    CHECK_EQ(loc.min_addr, 0x800);
    CHECK_EQ(loc.max_addr, 0xFFF);
}

static void test_mar_zone_cs(void) {
    uint64_t ram_last = 0;
    uint64_t rom_first = 0;
    uint64_t rom_last = 0;
    AddrRange ranges[8];
    int count = 0;
    CHECK_EQ(address_bits(64ull << 20), 26);
    CHECK_EQ(address_bits(32ull << 20), 25);
    CHECK(zone_rom(30, 3, 1, &ram_last, &rom_first, &rom_last) == 0);
    CHECK_EQ(ram_last, 0x2FFFFFFFull);
    CHECK_EQ(rom_first, 0x30000000ull);
    CHECK_EQ(rom_last, 0x3FFFFFFFull);
    CHECK(cs_ranges(16, 13, "001", ranges, 8, &count) == 0);
    CHECK_EQ(count, 1);
    CHECK_EQ(ranges[0].start, 0x2000);
    CHECK_EQ(ranges[0].end, 0x3FFF);
    CHECK(cs_ranges(16, 13, "0xx", ranges, 8, &count) == 0);
    CHECK_EQ(count, 4);
    CHECK_EQ(ranges[0].start, 0);
    CHECK_EQ(ranges[3].start, 0x6000);
    CHECK_EQ(ranges[3].end, 0x7FFF);
}

static void test_cross(void) {
    uint64_t addrs[] = {8005, 8006, 8007, 8008, 8001, 8002, 8003, 8004, 8000};
    int left[8];
    int right[8];
    int pairs = 0;
    CHECK(bank_pairs(4, addrs, 9, left, right, 8, &pairs) == 0);
    CHECK_EQ(pairs, 1);
    CHECK_EQ(addrs[left[0]], 8004);
    CHECK_EQ(addrs[right[0]], 8000);
    CHECK_EQ(span_cycles(4, 0x804001Aull, 8), 3);
    CHECK_EQ(span_cycles(4, 0, 8), 2);
    CHECK_EQ(llround(seq_seconds(1, 4, 200e-9, 50e-9, 4) * 1e9), 350);
    CHECK_EQ(llround(seq_seconds(0, 4, 200e-9, 50e-9, 4) * 1e9), 800);
    CHECK_EQ(llround(seq_seconds(1, 2, 200e-9, 50e-9, 4) * 1e9), 450);
    CHECK(seq_pipeline_ok(4, 200e-9, 50e-9));
    CHECK(!seq_pipeline_ok(2, 200e-9, 50e-9));
    CHECK_EQ(llround(channel_bytes_per_s(3, 1333e6, 64) / 1e6), 31992);
}

static void test_disk(void) {
    DiskTime time;
    disk_read_time(10000, 0.006, 4096, 20e6, 0.0002, &time);
    CHECK_EQ(llround(time.rot_s * 1e3), 3);
    CHECK_EQ(llround(time.total_s * 1e7), 94048);
    disk_read_time(10000, 0.006, 4000, 20e6, 0.0002, &time);
    CHECK_EQ(llround(time.total_s * 1e4), 94);
    disk_sector_time(7200, 0.008, 1000, &time);
    CHECK_EQ(llround(time.total_s * 1e6), 12175);
    CHECK_EQ(disk_capacity(2, 500, 32, 512), 16384000);
    CHECK_EQ(llround(disk_media_rate(7200, 1000, 512)), 61440000);
}

int main(void) {
    test_units();
    test_dram_chips();
    test_refresh();
    test_expand_and_locate();
    test_mar_zone_cs();
    test_cross();
    test_disk();
    return TEST_SUMMARY();
}
