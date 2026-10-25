#include <string.h>

#include "capacity.h"
#include "test.h"
#include "unit.h"
#include "wave.h"

static int unpack(const char *packed, char cells[][3]) {
    int n = (int)strlen(packed) / 2;
    int i;

    for (i = 0; i < n; i++) {
        cells[i][0] = packed[2 * i];
        cells[i][1] = packed[2 * i + 1];
        cells[i][2] = '\0';
    }
    return n;
}

static void test_units(void) {
    long long hz = 0;

    CHECK(parse_hz(3, "kHz", &hz));
    CHECK_EQ(hz, 3000);
    CHECK(parse_hz(200, "kHz", &hz));
    CHECK_EQ(hz, 200000);
    /* k 是 10^3。用 2^10 会把 200 kHz 算成 204800 Hz */
    CHECK(hz != 200 * 1024);
    CHECK(parse_hz(4, "MHz", &hz));
    CHECK_EQ(hz, 4000000LL);
    CHECK(parse_hz(8, "kHz", &hz));
    CHECK_EQ(hz, 8000);
    CHECK(parse_bps(24, "kbps", &hz));
    CHECK_EQ(hz, 24000);
    CHECK(parse_bps(48, "Mbps", &hz));
    CHECK_EQ(hz, 48000000LL);
    CHECK(parse_bps(10, "Mbps", &hz));
    CHECK_EQ(hz, 10000000LL);
    CHECK(parse_baud(20, "MBaud", &hz));
    CHECK_EQ(hz, 20000000LL);
    CHECK(!parse_hz(3, "KHz", &hz));
    CHECK(!parse_bps(1, "MB/s", &hz));
}

static void test_rates(void) {
    Symbol sym;
    ConvertOut conv;
    CodeOut code;
    NyquistOut nyq;

    CHECK(symbol_from_phase(4, 4, &sym));
    CHECK_EQ(sym.states, 16);
    CHECK_EQ(sym.bits, 4);
    CHECK(nyquist_compute(3000, &sym, &nyq));
    CHECK_EQ(nyq.max_baud, 6000);
    CHECK_EQ(nyq.max_bps, 24000);

    CHECK(symbol_from_states(4, &sym));
    CHECK(convert_rate(1, 2400, &sym, &conv));
    CHECK_EQ(conv.baud, 1200);
    CHECK_EQ(conv.bps, 2400);
    CHECK(!convert_rate(1, 1001, &sym, &conv));
    CHECK(!symbol_from_states(6, &sym));
    CHECK(!symbol_from_phase(3, 3, &sym));

    CHECK(symbol_from_states(4, &sym));
    CHECK(nyquist_compute(200000, &sym, &nyq));
    CHECK_EQ(nyq.max_baud, 400000);
    CHECK_EQ(nyq.max_bps, 800000);

    CHECK(nyquist_solve(4000000LL, 48000000LL, &nyq));
    CHECK_EQ(nyq.max_baud, 8000000LL);
    CHECK_EQ(nyq.symbol.bits, 6);
    CHECK_EQ(nyq.symbol.states, 64);
    CHECK(!nyquist_solve(4000000LL, 48000001LL, &nyq));

    CHECK(convert_code(1, 10000000LL, 1, &code));
    CHECK_EQ(code.baud, 20000000LL);
    CHECK_EQ(code.bps, 10000000LL);
    CHECK(convert_code(0, 20000000LL, 1, &code));
    CHECK_EQ(code.bps, 10000000LL);
    CHECK(convert_code(1, 10000000LL, 0, &code));
    CHECK_EQ(code.baud, 10000000LL);
}

static void test_shannon(void) {
    ShannonOut by_db;
    ShannonOut by_ratio;
    ShannonOut range;
    NeedOut need;

    CHECK(shannon_compute(8000, 1, 30, 0, 1, 50, &by_db));
    CHECK_EQ(by_db.kind, SHANNON_APPROX);
    CHECK_EQ(by_db.log_bits, 10);
    CHECK_EQ(by_db.snr, 1000);
    CHECK_EQ(by_db.one_plus, 1001);
    CHECK_EQ(by_db.theory_bps, 80000);
    CHECK_EQ(by_db.actual_bps, 40000);

    CHECK(shannon_compute(8000, 0, 0, 1000, 0, 100, &by_ratio));
    CHECK_EQ(by_ratio.theory_bps, by_db.theory_bps);

    CHECK(shannon_compute(4000, 0, 0, 15, 0, 100, &by_ratio));
    CHECK_EQ(by_ratio.kind, SHANNON_EXACT);
    CHECK_EQ(by_ratio.log_bits, 4);
    CHECK_EQ(by_ratio.theory_bps, 16000);

    CHECK(shannon_compute(3000, 0, 0, 10, 0, 100, &range));
    CHECK_EQ(range.kind, SHANNON_RANGE);
    CHECK(!shannon_compute(3000, 0, 0, 10, 1, 50, &range));

    CHECK(states_need(1, 30, 0, &need));
    CHECK_EQ(need.min_v, 32);
    CHECK_EQ(need.min_pow2, 32);
    CHECK_EQ(need.prev_sq, 961);
    CHECK_EQ(need.min_sq, 1024);

    CHECK(states_need(0, 0, 4, &need));
    CHECK_EQ(need.info.one_plus, 5);
    CHECK_EQ(need.min_v, 3);
    CHECK_EQ(need.min_pow2, 4);
    CHECK_EQ(need.info.kind, SHANNON_RANGE);
}

static void test_bound(void) {
    Symbol sym;
    BoundOut bound;

    CHECK(symbol_from_states(4, &sym));
    CHECK(bound_compute(3000, &sym, 0, 0, 3, &bound));
    CHECK_EQ(bound.nyquist.max_bps, 12000);
    CHECK_EQ(bound.shannon.theory_bps, 6000);
    CHECK_EQ(bound.which, 1);
    CHECK_EQ(bound.limit_bps, 6000);

    CHECK(symbol_from_states(2, &sym));
    CHECK(bound_compute(4000, &sym, 0, 0, 15, &bound));
    CHECK_EQ(bound.nyquist.max_bps, 8000);
    CHECK_EQ(bound.shannon.theory_bps, 16000);
    CHECK_EQ(bound.which, 0);

    CHECK(symbol_from_states(32, &sym));
    CHECK(bound_compute(8000, &sym, 1, 30, 0, &bound));
    CHECK_EQ(bound.which, 2);
    CHECK_EQ(bound.limit_bps, 80000);
}

static void join_cells(const char *src[], int n, char cells[][3], char *packed) {
    int i;

    packed[0] = '\0';
    for (i = 0; i < n; i++) {
        cells[i][0] = src[i][0];
        cells[i][1] = src[i][1];
        cells[i][2] = '\0';
        strcat(packed, src[i]);
    }
}

static void test_wave(void) {
    char nrz[WAVE_MAX * 2 + 1];
    char man[WAVE_MAX * 2 + 1];
    char diff[WAVE_MAX * 2 + 1];
    char bits[WAVE_MAX + 1];
    char cells[8][3];
    char packed[17];
    int n;
    const char *man2013[] = {"_-", "_-", "-_", "-_", "_-", "-_", "-_", "_-"};
    const char *diff_bits[] = {"_-", "_-", "-_", "_-", "-_", "-_", "-_", "_-"};

    CHECK(wave_encode("10", LEVEL_LOW, nrz, man, diff));
    CHECK_STR(nrz, "--__");
    CHECK_STR(man, "-__-");
    CHECK_STR(diff, "_-_-");
    n = unpack(diff, cells);
    CHECK(wave_decode_diff(cells, n, LEVEL_LOW, bits));
    CHECK_STR(bits, "10");

    join_cells(man2013, 8, cells, packed);
    CHECK(wave_encode("00110110", LEVEL_LOW, nrz, man, diff));
    CHECK_STR(man, packed);
    CHECK(wave_decode_man(cells, 8, bits));
    CHECK_STR(bits, "00110110");

    join_cells(diff_bits, 8, cells, packed);
    CHECK(wave_encode("10111001", LEVEL_LOW, nrz, man, diff));
    CHECK_STR(diff, packed);
    CHECK(wave_decode_diff(cells, 8, LEVEL_LOW, bits));
    CHECK_STR(bits, "10111001");

    CHECK(wave_encode("01100111", LEVEL_LOW, nrz, man, diff));
    n = unpack(nrz, cells);
    CHECK(wave_decode_nrz(cells, n, bits));
    CHECK_STR(bits, "01100111");
    n = unpack(man, cells);
    CHECK(wave_decode_man(cells, n, bits));
    CHECK_STR(bits, "01100111");
    n = unpack(diff, cells);
    CHECK(wave_decode_diff(cells, n, LEVEL_LOW, bits));
    CHECK_STR(bits, "01100111");

    cells[0][0] = '-';
    cells[0][1] = '-';
    cells[0][2] = '\0';
    CHECK(!wave_decode_man(cells, 1, bits));
    CHECK(wave_decode_nrz(cells, 1, bits));
    CHECK_STR(bits, "1");
    CHECK(!wave_encode("2", LEVEL_LOW, nrz, man, diff));
}

int main(void) {
    test_units();
    test_rates();
    test_shannon();
    test_bound();
    test_wave();
    return TEST_SUMMARY();
}
