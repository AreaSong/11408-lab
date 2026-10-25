#include <string.h>

#include "crcdiv.h"
#include "frame.h"
#include "hamming.h"
#include "test.h"

static void test_bits_and_bytes(void) {
    BitStr bits;
    ByteStr bytes;
    char err[80];
    char text[80];

    CHECK(parse_bits("0111 1100", &bits, err, 80));
    CHECK_EQ(bits.len, 8);
    CHECK_STR(bits.bit, "01111100");
    CHECK(!parse_bits("012", &bits, err, 80));
    CHECK(parse_bytes("7e 21 7D", &bytes, err, 80));
    CHECK_EQ(bytes.len, 3);
    CHECK_EQ(bytes.byte[0], 0x7E);
    CHECK_EQ(bytes.byte[2], 0x7D);
    CHECK(format_bytes(bytes.byte, bytes.len, text, 80));
    CHECK_STR(text, "7E 21 7D");
    CHECK(!parse_bytes("7", &bytes, err, 80));
}

static void test_count_and_byte(void) {
    ByteStr payload;
    ByteStr frame;
    ByteStr back;
    CountFrame frames[4];
    unsigned char raw[128];
    char err[80];
    int n = 0;
    int i;

    CHECK(parse_bytes("01 02 03 04", &payload, err, 80));
    CHECK(count_send(&payload, &frame, err, 80));
    CHECK_EQ(frame.byte[0], 5);
    CHECK_EQ(frame.len, 5);
    payload.len = 0;
    CHECK(count_send(&payload, &frame, err, 80));
    CHECK_EQ(frame.byte[0], 1);

    CHECK(parse_bytes("06 AA BB 03 CC DD 03 EE FF", &payload, err, 80));
    CHECK(count_recv(&payload, frames, 4, &n, err, 80));
    CHECK_EQ(n, 2);
    CHECK_EQ(frames[0].data_len, 5);
    CHECK_EQ(frames[0].data[2], 0x03);
    CHECK_EQ(frames[1].data_len, 2);
    CHECK_EQ(frames[1].data[0], 0xEE);

    for (i = 0; i < 128; i++) {
        raw[i] = (unsigned char)i;
    }
    payload.len = 128;
    memcpy(payload.byte, raw, 128);
    CHECK(byte_stuff(&payload, &frame, err, 80));
    CHECK(byte_unstuff(&frame, &back, err, 80));
    CHECK_EQ(back.len, 128);
    CHECK(memcmp(back.byte, raw, 128) == 0);
    CHECK(parse_bytes("7E 21 7D 01 41", &payload, err, 80));
    CHECK(byte_stuff(&payload, &frame, err, 80));
    CHECK_EQ(frame.len, 8);
    CHECK_EQ(frame.byte[0], 0x7D);
    CHECK_EQ(frame.byte[1], 0x5E);
    CHECK_EQ(frame.byte[4], 0x5D);
    CHECK_EQ(frame.byte[6], 0x21);
}

static void test_bit_and_parity(void) {
    char out[80];
    int out_len = 0;
    int after[8];
    int ninsert = 0;
    int deleted[8];
    int ndeleted = 0;
    int ones = 0;
    int ok = 0;
    char err[80];
    BitStr bodies[4];
    int nframes = 0;
    const char *data = "0111110001111110";

    CHECK(bit_stuff(data, 16, out, 80, &out_len, after, &ninsert, err, 80));
    CHECK_STR(out, "011111000011111010");
    CHECK_EQ(ninsert, 2);
    CHECK_EQ(after[0], 6);
    CHECK_EQ(after[1], 14);
    CHECK(bit_unstuff(out, out_len, out, 80, &out_len, deleted, &ndeleted, err, 80));
    CHECK_STR(out, data);
    CHECK_EQ(deleted[0], 7);
    CHECK_EQ(deleted[1], 16);
    CHECK(bit_stuff("1111111111", 10, out, 80, &out_len, after, &ninsert, err, 80));
    CHECK_STR(out, "111110111110");

    CHECK(parity_make("1011001", 7, 0, out, 80, &out_len, err, 80));
    CHECK_STR(out, "10110010");
    CHECK(parity_make("1011001", 7, 1, out, 80, &out_len, err, 80));
    CHECK_STR(out, "10110011");
    parity_check("11100010", 8, 0, &ones, &ok);
    CHECK_EQ(ones, 4);
    CHECK(ok);
    parity_check("11110010", 8, 0, &ones, &ok);
    CHECK(!ok);

    CHECK(bit_split("011111100111110000111110100111111011111001111110",
                    48, bodies, 4, &nframes, err, 80));
    CHECK_EQ(nframes, 2);
    CHECK_EQ(bodies[0].len, 18);
    CHECK_EQ(bodies[1].len, 6);
}

static void expect_crc(const char *gen_text, const char *data, int append, const char *rem) {
    CrcWork work;
    char gen[CRC_GEN_CAP];
    char err[80];

    CHECK(crc_parse_gen(gen_text, gen, CRC_GEN_CAP, err, 80));
    CHECK(crc_divide(data, (int)strlen(data), gen, append, &work, err, 80));
    CHECK_STR(work.remainder, rem);
    if (append) {
        CHECK(crc_divide(work.sent, (int)strlen(work.sent), gen, 0, &work, err, 80));
        CHECK(work.clean);
    }
}

static void test_crc(void) {
    char gen[CRC_GEN_CAP];
    char err[80];

    CHECK(crc_parse_gen("x^4 + x + 1", gen, CRC_GEN_CAP, err, 80));
    CHECK_STR(gen, "10011");
    CHECK(crc_parse_gen("x3+x2+1", gen, CRC_GEN_CAP, err, 80));
    CHECK_STR(gen, "1101");
    CHECK(!crc_parse_gen("10010", gen, CRC_GEN_CAP, err, 80));
    expect_crc("10011", "101110000", 0, "1100");
    expect_crc("10011", "101110100", 0, "1000");
    expect_crc("10011", "101111000", 0, "0100");
    expect_crc("10011", "101111100", 0, "0000");
    expect_crc("1101", "101001", 1, "001");
    expect_crc("1011", "1010", 1, "011");
    expect_crc("111", "110011", 1, "01");
}

static void test_one_flip(const char *data, int odd) {
    HamCode encoded;
    char err[80];
    int i;

    CHECK(hamming_encode(data, (int)strlen(data), odd, &encoded, err, 80));
    CHECK_EQ(encoded.syndrome, 0);
    for (i = 1; i <= encoded.n; i++) {
        HamCode fixed;
        char bad[HAM_CAP];
        memcpy(bad, encoded.code, (size_t)encoded.n + 1);
        bad[i - 1] = bad[i - 1] == '0' ? '1' : '0';
        CHECK(hamming_correct(bad, encoded.n, odd, &fixed, err, 80));
        CHECK_EQ(fixed.syndrome, i);
        CHECK_STR(fixed.fixed, encoded.code);
    }
}

static void test_hamming(void) {
    HamCode code;
    char err[80];
    int r = 0;

    CHECK(hamming_need(8, &r, err, 80));
    CHECK_EQ(r, 4);
    CHECK(hamming_need(4, &r, err, 80));
    CHECK_EQ(r, 3);
    CHECK(hamming_need(11, &r, err, 80));
    CHECK_EQ(r, 4);
    CHECK(hamming_need(12, &r, err, 80));
    CHECK_EQ(r, 5);
    CHECK(hamming_need(1, &r, err, 80));
    CHECK_EQ(r, 2);
    CHECK(hamming_encode("1010", 4, 0, &code, err, 80));
    CHECK_STR(code.code, "1011010");
    CHECK(hamming_encode("1011", 4, 0, &code, err, 80));
    CHECK_STR(code.code, "0110011");
    CHECK(hamming_encode("1010", 4, 1, &code, err, 80));
    CHECK_STR(code.code, "0110010");
    test_one_flip("1010", 0);
    test_one_flip("10110011", 0);
    test_one_flip("1010", 1);
}

int main(void) {
    test_bits_and_bytes();
    test_count_and_byte();
    test_bit_and_parity();
    test_crc();
    test_hamming();
    return TEST_SUMMARY();
}
