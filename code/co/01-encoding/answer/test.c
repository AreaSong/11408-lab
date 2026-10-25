#include "bits.h"
#include "code.h"
#include "mem.h"
#include "number.h"
#include "radix.h"
#include "test.h"

#include <string.h>

static void expect_code(int width, long long value, CodeKind kind, unsigned long long bits) {
    Bits out;
    CHECK(code_encode(width, value, kind, &out));
    CHECK_EQ(out.bits, bits);
}

static void test_four_bit_table(void) {
    static const struct {
        long long value;
        unsigned orig;
        unsigned ones;
        unsigned comp;
        unsigned bias;
        int orig_ok;
    } table[] = {
        {0, 0x0, 0x0, 0x0, 0x8, 1},  {1, 0x1, 0x1, 0x1, 0x9, 1},  {7, 0x7, 0x7, 0x7, 0xF, 1},
        {-1, 0x9, 0xE, 0xF, 0x7, 1}, {-7, 0xF, 0x8, 0x9, 0x1, 1}, {-8, 0, 0, 0x8, 0x0, 0},
    };
    int i;
    for (i = 0; i < 6; i++) {
        Bits out;
        if (table[i].orig_ok) {
            expect_code(4, table[i].value, KIND_ORIG, table[i].orig);
            expect_code(4, table[i].value, KIND_ONES, table[i].ones);
        } else {
            CHECK(!code_encode(4, table[i].value, KIND_ORIG, &out));
            CHECK(!code_encode(4, table[i].value, KIND_ONES, &out));
        }
        expect_code(4, table[i].value, KIND_COMP, table[i].comp);
        expect_code(4, table[i].value, KIND_BIAS, table[i].bias);
    }
}

static void test_exam_patterns(void) {
    Bits src;
    Bits ext;
    long long value = 0;
    unsigned long long uns = 0;
    bool as_unsigned = false;
    bool minus_zero = false;
    char err[80];
    expect_code(8, -45, KIND_ORIG, 0xAD);
    expect_code(8, -45, KIND_ONES, 0xD2);
    expect_code(8, -45, KIND_COMP, 0xD3);
    expect_code(8, -45, KIND_BIAS, 0x53);
    expect_code(8, -128, KIND_COMP, 0x80);
    expect_code(8, -128, KIND_BIAS, 0x00);
    int carry = 0;
    expect_code(16, -9, KIND_COMP, 0xFFF7);
    CHECK(bits_parse("FFF7H", 16, &src, err, sizeof err));
    ext = code_extend(src, 32, true);
    CHECK_EQ(ext.bits, 0xFFFFFFF7ULL);
    CHECK_EQ(code_add(32, 0x7F, 0xFFFFFFF7ULL, &carry), 0x76ULL);
    CHECK_EQ(carry, 1);
    CHECK(code_retype(false, false, 16, 32, 65530, &ext, &value, &uns, &as_unsigned, err, sizeof err));
    CHECK(as_unsigned);
    CHECK_EQ(ext.bits, 0xFFFAULL);
    CHECK(code_retype(true, true, 32, 16, 32777, &ext, &value, &uns, &as_unsigned, err, sizeof err));
    CHECK_EQ(ext.bits, 0x8009ULL);
    CHECK_EQ(value, -32759);
    CHECK(code_retype(true, false, 16, 32, -32767, &ext, &value, &uns, &as_unsigned, err, sizeof err));
    CHECK_EQ(ext.bits, 0xFFFF8001ULL);
    CHECK_EQ(uns, 4294934529ULL);
    CHECK(code_decode(bits_make(16, 0xFFFD), KIND_COMP, &value, &minus_zero));
    CHECK_EQ(value, -3);
    CHECK(code_decode(bits_make(16, 0xFFDF), KIND_COMP, &value, &minus_zero));
    CHECK_EQ(value, -33);
    CHECK(code_decode(bits_make(16, 0x7FFC), KIND_UNSIGNED, &value, &minus_zero));
    CHECK_EQ(value, 32764);
    CHECK_EQ(code_comp_min(32), -2147483648LL);
    CHECK_EQ(code_comp_max(32), 2147483647LL);
}

static void test_fraction_and_radix(void) {
    Decimal d;
    long long units = 0;
    char err[80];
    char text[64];
    bool exact = false;
    CHECK(decimal_parse("-0.75", &d, err, sizeof err));
    CHECK(code_frac_units(8, &d, &units, err, sizeof err));
    CHECK_EQ(units, -96);
    expect_code(8, units, KIND_COMP, 0xA0);
    expect_code(8, units, KIND_ORIG, 0xE0);
    CHECK(decimal_parse("-1", &d, err, sizeof err));
    CHECK(code_frac_units(8, &d, &units, err, sizeof err));
    CHECK_EQ(units, -128);
    CHECK(radix_result(10, 2, 8, "13.75", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK(exact);
    CHECK_STR(text, "1101.11");
    CHECK(radix_result(10, 8, 4, "13.75", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK_STR(text, "15.6");
    CHECK(radix_result(10, 16, 4, "13.75", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK_STR(text, "D.C");
    CHECK(radix_result(2, 10, 8, "1101.11", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK_STR(text, "13.75");
    CHECK(radix_result(16, 2, 8, "D.C", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK_STR(text, "1101.11");
    CHECK(radix_result(10, 2, 8, "0.1", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK(!exact);
    CHECK_STR(text, "0.00011001");
    CHECK(radix_result(10, 16, 0, "-45", text, sizeof text, &exact, err, sizeof err) == 0);
    CHECK_STR(text, "-2D");
}

static void test_tail_padding(void) {
    Member m[3];
    int align = 0;
    int total = 0;
    memset(m, 0, sizeof m);
    m[0].size = 1;
    m[0].align = 1;
    m[1].size = 4;
    m[1].align = 4;
    m[2].size = 1;
    m[2].align = 1;
    mem_layout(m, 3, &align, &total);
    CHECK_EQ(total, 12);
    CHECK_EQ(m[1].offset, 4);
    CHECK_EQ(m[2].offset, 8);
}

static void test_memory(void) {
    Member m[3];
    int align = 0;
    int total = 0;
    memset(m, 0, sizeof m);
    m[0].size = 4;
    m[0].align = 4;
    m[0].has_value = true;
    m[0].machine = 273;
    m[1].size = 1;
    m[1].align = 1;
    m[2].size = 2;
    m[2].align = 2;
    mem_layout(m, 3, &align, &total);
    CHECK_EQ(total, 8);
    CHECK_EQ(m[2].offset, 6);
    CHECK_EQ(mem_byte(273, 4, true, 0), 0x11);
    memset(m, 0, sizeof m);
    m[0].size = 2;
    m[0].align = 2;
    m[1].size = 4;
    m[1].align = 4;
    m[1].has_value = true;
    m[1].machine = 0x12340000ULL;
    mem_layout(m, 2, &align, &total);
    CHECK_EQ(m[1].offset, 4);
    CHECK_EQ(mem_byte(m[1].machine, 4, true, 2), 0x34);
    memset(m, 0, sizeof m);
    m[0].size = 4;
    m[0].align = 4;
    m[1].size = 10;
    m[1].align = 1;
    m[2].size = 4;
    m[2].align = 4;
    mem_layout(m, 3, &align, &total);
    CHECK_EQ(total, 20);
    CHECK_EQ(m[2].offset, 16);
    CHECK_EQ(mem_byte(0x12345678ULL, 4, true, 1), 0x56);
    CHECK_EQ(mem_byte(0x1122334455667788ULL, 8, true, 6), 0x22);
}

int main(void) {
    test_four_bit_table();
    test_exam_patterns();
    test_fraction_and_radix();
    test_memory();
    test_tail_padding();
    return TEST_SUMMARY();
}
