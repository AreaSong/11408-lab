#include "code.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void show_bits(FILE *out, const char *label, Bits b, bool point) {
    char body[80];
    char hex[40];
    if (point) {
        bits_write_point(body, sizeof body, b);
    } else {
        bits_write_bin(body, sizeof body, b);
    }
    bits_write_hex(hex, sizeof hex, b);
    fprintf(out, "%s：%s（%s）\n", label, body, hex);
}

static void value_text(char *dst, size_t n, int width, long long units, bool frac) {
    char body[64];
    unsigned long long scale;
    unsigned long long abs_units;
    if (!frac) {
        snprintf(dst, n, "%lld", units);
        return;
    }
    scale = 1ULL << (width - 1);
    abs_units = units < 0 ? (unsigned long long)(-units) : (unsigned long long)units;
    format_ratio(abs_units, scale, body, sizeof body);
    snprintf(dst, n, "%s%s", units < 0 ? "-" : "", body);
}

static void result_codes(FILE *out, int width, long long value, bool point) {
    Bits orig, ones, comp, bias;
    char a[40], b[40], c[40], d[40];
    bool orig_ok = code_encode(width, value, KIND_ORIG, &orig);
    code_encode(width, value, KIND_ONES, &ones);
    code_encode(width, value, KIND_COMP, &comp);
    code_encode(width, value, KIND_BIAS, &bias);
    if (point) {
        bits_write_point(c, sizeof c, comp);
        bits_write_point(d, sizeof d, bias);
    } else {
        bits_write_bin(c, sizeof c, comp);
        bits_write_bin(d, sizeof d, bias);
    }
    if (!orig_ok) {
        fprintf(out, "结果：补码 %s，移码 %s；原码和反码无法表示\n", c, d);
        return;
    }
    if (point) {
        bits_write_point(a, sizeof a, orig);
        bits_write_point(b, sizeof b, ones);
    } else {
        bits_write_bin(a, sizeof a, orig);
        bits_write_bin(b, sizeof b, ones);
    }
    fprintf(out, "结果：原码 %s，反码 %s，补码 %s，移码 %s\n", a, b, c, d);
}

static void show_regular(FILE *out, int width, long long value, bool point) {
    Bits orig, ones, comp, bias;
    char mag[40];
    Bits mag_bits;
    code_encode(width, value, KIND_ORIG, &orig);
    code_encode(width, value, KIND_ONES, &ones);
    code_encode(width, value, KIND_COMP, &comp);
    code_encode(width, value, KIND_BIAS, &bias);
    if (value < 0) {
        mag_bits = bits_make(width - 1, (unsigned long long)(-value));
        bits_write_bin(mag, sizeof mag, mag_bits);
        fprintf(out, "绝对值的 %d 位数值是 %s。负数先写原码，再取反、加 1\n", width - 1, mag);
    } else {
        fprintf(out, "正数的原码、反码、补码相同；移码只把符号位取反\n");
    }
    show_bits(out, "原码", orig, point);
    show_bits(out, "反码", ones, point);
    show_bits(out, "补码", comp, point);
    show_bits(out, "移码", bias, point);
    if (!point && value < 0) {
        fprintf(out, "补码定义式：2^%d + (%lld) = %llu\n", width, value,
                (unsigned long long)value & bits_mask(width));
        fprintf(out, "移码 = 补码符号位取反，也就是加偏置 2^%d\n", width - 1);
    }
    if (point) {
        fprintf(out, "小数移码的偏置是 1，位模式仍是补码符号位取反\n");
    }
}

static void show_minimum(FILE *out, int width, long long value, bool point) {
    Bits comp;
    Bits bias;
    code_encode(width, value, KIND_COMP, &comp);
    code_encode(width, value, KIND_BIAS, &bias);
    if (point) {
        fprintf(out, "这是补码小数的下界 -1。原码和反码最负只到 -(1 - 2^-%d)，没有 -1\n", width - 1);
    } else {
        fprintf(out, "这个真值是 -2^%d，只有补码和移码有码点\n", width - 1);
        fprintf(out, "原码、反码：无法表示（最小只能到 %lld）\n", value + 1);
    }
    show_bits(out, "补码", comp, point);
    show_bits(out, "移码", bias, point);
    fprintf(out, "移码全 0 是最小真值，不是零\n");
}

static void show_zero(FILE *out, int width, bool minus_zero, bool point) {
    Bits plus, bias, neg_orig, neg_ones;
    plus = bits_make(width, 0);
    bias = bits_make(width, 1ULL << (width - 1));
    neg_orig = bits_make(width, 1ULL << (width - 1));
    neg_ones = bits_make(width, bits_mask(width));
    if (minus_zero) {
        fprintf(out, "输入的是 -0。补码和移码没有负零\n");
    } else {
        fprintf(out, "输入的 0 按 +0 编码。原码和反码另外还有一个 -0\n");
    }
    show_bits(out, "原码 +0", plus, point);
    show_bits(out, "原码 -0", neg_orig, point);
    show_bits(out, "反码 +0", plus, point);
    show_bits(out, "反码 -0", neg_ones, point);
    show_bits(out, "补码 0", plus, point);
    show_bits(out, "移码 0", bias, point);
    fprintf(out, "结果：补码的零是全 0，移码的零是最高位为 1\n");
}

static int print_units(FILE *out, int width, long long units, bool frac, bool minus_zero) {
    char shown[64];
    value_text(shown, sizeof shown, width, units, frac);
    fprintf(out, "【%d 位%s】真值 %s\n", width, frac ? "定点小数" : "整数", shown);
    if (frac) {
        fprintf(out, "小数点紧挨在符号位后面，数值位 %d 位\n", width - 1);
    }
    if (units == 0) {
        show_zero(out, width, minus_zero, frac);
        return 0;
    }
    if (units == code_comp_min(width)) {
        show_minimum(out, width, units, frac);
    } else {
        show_regular(out, width, units, frac);
    }
    result_codes(out, width, units, frac);
    return 0;
}

int code_print_int(FILE *out, int width, const char *text) {
    Decimal d;
    long long value = 0;
    char err[160];
    if (!code_width_ok(width)) {
        return fail("位宽要在 2 到 32 之间");
    }
    if (!decimal_parse(text, &d, err, sizeof err)) {
        return fail(err);
    }
    if (!decimal_is_int(&d) || !decimal_to_i64(&d, &value)) {
        return fail("整数编码只接受整数真值");
    }
    if (!code_in_comp(width, value)) {
        snprintf(err, sizeof err, "真值 %lld 超出 %d 位补码范围", value, width);
        return fail(err);
    }
    return print_units(out, width, value, false, d.minus_zero);
}

int code_print_frac(FILE *out, int width, const char *text) {
    Decimal d;
    long long units = 0;
    char err[160];
    if (!decimal_parse(text, &d, err, sizeof err)) {
        return fail(err);
    }
    if (!code_frac_units(width, &d, &units, err, sizeof err)) {
        return fail(err);
    }
    return print_units(out, width, units, true, d.minus_zero);
}

int code_print_decode(FILE *out, bool frac, int width, CodeKind kind, const char *pattern) {
    Bits b;
    long long units = 0;
    bool minus_zero = false;
    char err[160];
    char shown[64];
    if (!bits_parse(pattern, width, &b, err, sizeof err)) {
        return fail(err);
    }
    if (!code_decode(b, kind, &units, &minus_zero)) {
        return fail("这串位解不出来");
    }
    value_text(shown, sizeof shown, width, units, frac);
    fprintf(out, "【%s求真值】机器数按 %s 读\n", frac ? "定点小数" : "整数", code_kind_name(kind));
    if (minus_zero) {
        fprintf(out, "这是%s的 -0，数值是 0，但机器数不是全 0\n", code_kind_name(kind));
        fprintf(out, "结果：-0\n");
        return 0;
    }
    fprintf(out, "真值 %s\n", shown);
    return print_units(out, width, units, frac, false);
}
