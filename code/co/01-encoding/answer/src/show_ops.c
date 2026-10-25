#include "code.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void sort3(const long long v[3], int order[3]) {
    int i;
    int j;
    order[0] = 0;
    order[1] = 1;
    order[2] = 2;
    for (i = 0; i < 3; i++) {
        for (j = i + 1; j < 3; j++) {
            if (v[order[j]] < v[order[i]]) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }
}

static void format_order(char *dst, size_t n, const long long v[3]) {
    static const char *name[] = {"x", "y", "z"};
    int order[3];
    int i;
    size_t k = 0;
    sort3(v, order);
    dst[0] = '\0';
    for (i = 0; i < 3 && k + 1 < n; i++) {
        const char *sep = "";
        if (i > 0) {
            sep = v[order[i]] == v[order[i - 1]] ? " = " : " < ";
        }
        k += (size_t)snprintf(dst + k, n - k, "%s%s", sep, name[order[i]]);
    }
}

int code_print_range(FILE *out, bool frac, int width) {
    char max_frac[64];
    char lo_hex[40];
    char hi_hex[40];
    char zero_bias[40];
    unsigned long long scale;
    Bits lo;
    Bits hi;
    Bits bz;
    if (!code_width_ok(width)) {
        return fail("位宽要在 2 到 32 之间");
    }
    scale = 1ULL << (width - 1);
    lo = bits_make(width, scale);
    hi = bits_make(width, scale - 1ULL);
    bz = bits_make(width, scale);
    bits_write_hex(lo_hex, sizeof lo_hex, lo);
    bits_write_hex(hi_hex, sizeof hi_hex, hi);
    bits_write_hex(zero_bias, sizeof zero_bias, bz);
    if (frac) {
        format_ratio(scale - 1ULL, scale, max_frac, sizeof max_frac);
        fprintf(out, "【%d 位定点小数的表示范围】数值位 %d 位，分辨率 2^-%d\n", width, width - 1,
                width - 1);
        fprintf(out, "原码、反码：-%s ～ %s，两个零\n", max_frac, max_frac);
        fprintf(out, "补码、移码：-1 ～ %s，一个零。比原码多出来的是 -1\n", max_frac);
        fprintf(out, "小数移码偏置 1；整数移码偏置 2^%d。都不是 IEEE 754 的 2^(k-1)-1\n", width - 1);
        fprintf(out, "结果：补码 -1 ～ %s\n", max_frac);
        return 0;
    }
    fprintf(out, "【%d 位整数的表示范围】\n", width);
    fprintf(out, "无符号：0 ～ %llu（0 ～ 2^%d-1），一个零\n", bits_mask(width), width);
    fprintf(out, "原码：%lld ～ %lld，两个零（+0 全 0，-0 只有符号位为 1）\n", -(code_comp_max(width)),
            code_comp_max(width));
    fprintf(out, "反码：范围与原码相同，-0 是全 1\n");
    fprintf(out, "补码：%lld ～ %lld（-2^%d ～ 2^%d-1），一个零\n", code_comp_min(width),
            code_comp_max(width), width - 1, width - 1);
    fprintf(out, "最小负数的补码是 %s，最大正数的补码是 %s\n", lo_hex, hi_hex);
    fprintf(out, "移码：范围与补码相同，偏置 2^%d。零是 %s，全 0 是最小负数\n", width - 1, zero_bias);
    fprintf(out, "阶码用的偏置是 2^(k-1)-1（单精度 127），那是第 04 关的事\n");
    fprintf(out, "结果：补码 %lld ～ %lld，即 -2^%d ～ 2^%d-1\n", code_comp_min(width),
            code_comp_max(width), width - 1, width - 1);
    return 0;
}

static int load_signed_value(const char *text, long long *value) {
    Decimal d;
    char err[160];
    if (!decimal_parse(text, &d, err, sizeof err) || !decimal_is_int(&d) || !decimal_to_i64(&d, value)) {
        return fail("转换这一步只接受整数真值");
    }
    return 0;
}

static void print_unsigned_result(FILE *out, int dst_w, long long signed_bits, unsigned long long out_u,
                                  const char *dst_hex) {
    int k;
    fprintf(out, "按无符号读：%llu\n", out_u);
    if (signed_bits < 0) {
        fprintf(out, "同一串位的补码是 %lld，所以无符号值 = 2^%d + (%lld)\n", signed_bits, dst_w, signed_bits);
        for (k = 1; k < dst_w; k++) {
            if (-signed_bits == (1LL << k) - 1) {
                fprintf(out, "%lld = -(2^%d - 1)，写成 2^%d - 2^%d + 1\n", signed_bits, k, dst_w, k);
                break;
            }
        }
    }
    fprintf(out, "结果：%llu，机器数 %s\n", out_u, dst_hex);
}

int code_print_retype(FILE *out, bool src_signed, bool dst_signed, int src_w, int dst_w, const char *text) {
    long long value = 0;
    Bits src;
    Bits stored;
    long long out_s = 0;
    unsigned long long out_u = 0;
    bool as_unsigned = false;
    long long signed_bits = 0;
    bool minus_zero = false;
    char err[160];
    char src_hex[40];
    char dst_hex[40];
    if (load_signed_value(text, &value) != 0) {
        return 1;
    }
    if (!code_encode(src_w, value, src_signed ? KIND_COMP : KIND_UNSIGNED, &src)) {
        snprintf(err, sizeof err, "真值 %lld 放不进 %s %d 位", value, src_signed ? "补码" : "无符号", src_w);
        return fail(err);
    }
    if (!code_retype(src_signed, dst_signed, src_w, dst_w, value, &stored, &out_s, &out_u, &as_unsigned,
                     err, sizeof err)) {
        return fail(err);
    }
    bits_write_hex(src_hex, sizeof src_hex, src);
    bits_write_hex(dst_hex, sizeof dst_hex, stored);
    code_decode(stored, KIND_COMP, &signed_bits, &minus_zero);
    fprintf(out, "【整型转换】%s %d 位 → %s %d 位，真值 %lld\n", src_signed ? "有符号" : "无符号", src_w,
            dst_signed ? "有符号" : "无符号", dst_w, value);
    fprintf(out, "源机器数 %s\n", src_hex);
    if (dst_w > src_w) {
        fprintf(out, "加宽时按源类型补位：%s → %s\n", src_signed ? "复制符号位" : "高位补 0", dst_hex);
    } else if (dst_w < src_w) {
        fprintf(out, "变窄时丢掉高位，留下低 %d 位 %s\n", dst_w, dst_hex);
    } else {
        fprintf(out, "位宽不变，机器数仍是 %s\n", dst_hex);
    }
    if (as_unsigned) {
        print_unsigned_result(out, dst_w, signed_bits, out_u, dst_hex);
        return 0;
    }
    fprintf(out, "按补码读：%lld\n", out_s);
    fprintf(out, "结果：%lld，机器数 %s\n", out_s, dst_hex);
    return 0;
}

static void print_orig_extend(FILE *out, Bits src, Bits extended, Bits copied, long long value, long long wrong) {
    char src_bin[80];
    char good[40];
    char bad[40];
    char good_bin[80];
    bits_write_bin(src_bin, sizeof src_bin, src);
    bits_write_bin(good_bin, sizeof good_bin, extended);
    bits_write_hex(good, sizeof good, extended);
    bits_write_hex(bad, sizeof bad, copied);
    fprintf(out, "原机器数 %s，按原码读是 %lld\n", src_bin, value);
    fprintf(out, "符号位移到新的最高位，中间数值位补 0：%s（%s），真值仍是 %lld\n", good_bin, good, value);
    fprintf(out, "若照补码把最高位复制下去，得到 %s，再按原码读是 %lld\n", bad, wrong);
    fprintf(out, "结果：%s，真值 %lld\n", good, value);
}

int code_print_orig_extend(FILE *out, int from_w, int to_w, const char *pattern) {
    Bits src;
    Bits extended;
    Bits copied;
    long long value = 0;
    long long wrong = 0;
    bool minus_zero = false;
    char err[160];
    if (!code_width_ok(from_w) || !code_width_ok(to_w) || to_w <= from_w) {
        return fail("原码扩展的目标位宽要更大，并且都在 2 到 32 之间");
    }
    if (!bits_parse(pattern, from_w, &src, err, sizeof err)) {
        return fail(err);
    }
    extended = code_extend_orig(src, to_w);
    copied = code_extend(src, to_w, true);
    code_decode(src, KIND_ORIG, &value, &minus_zero);
    code_decode(copied, KIND_ORIG, &wrong, &minus_zero);
    fprintf(out, "【原码扩展】%d 位 → %d 位\n", from_w, to_w);
    if (minus_zero) {
        fprintf(out, "这是原码的 -0，扩展后符号位仍单独留在最高位\n");
    }
    print_orig_extend(out, src, extended, copied, value, wrong);
    return 0;
}

int code_print_sum(FILE *out, int width, const char *a_text, const char *b_text) {
    Bits a;
    Bits b;
    Bits sum_bits;
    Bits wide;
    long long value = 0;
    bool minus_zero = false;
    int carry = 0;
    unsigned long long low;
    char err[160];
    char ha[40];
    char hb[40];
    char hs[40];
    if (!code_width_ok(width)) {
        return fail("位宽要在 2 到 32 之间");
    }
    if (!bits_parse(a_text, width, &a, err, sizeof err) || !bits_parse(b_text, width, &b, err, sizeof err)) {
        return fail(err);
    }
    low = code_add(width, a.bits, b.bits, &carry);
    sum_bits = bits_make(width, low);
    wide = bits_make(width + 1, (a.bits & bits_mask(width)) + (b.bits & bits_mask(width)));
    code_decode(sum_bits, KIND_COMP, &value, &minus_zero);
    bits_write_hex(ha, sizeof ha, a);
    bits_write_hex(hb, sizeof hb, b);
    bits_write_hex(hs, sizeof hs, sum_bits);
    fprintf(out, "【模 2^%d 相加】%s + %s\n", width, ha, hb);
    bits_write_hex(ha, sizeof ha, wide);
    fprintf(out, "含进位的和是 %s，丢掉最高位进位 %d，留下 %s\n", ha, carry, hs);
    fprintf(out, "按补码读是 %lld。标志位 ZF、SF、CF、OF 留到下一关\n", value);
    fprintf(out, "结果：%s，真值 %lld\n", hs, value);
    return 0;
}

int code_print_cmp(FILE *out, int width, const char *x, const char *y, const char *z) {
    const char *text[3] = {x, y, z};
    static const char *name[] = {"x", "y", "z"};
    Bits b[3];
    long long uns[3];
    long long sig[3];
    char order_u[32];
    char order_s[32];
    char err[160];
    char hex[40];
    int i;
    if (!code_width_ok(width)) {
        return fail("位宽要在 2 到 32 之间");
    }
    for (i = 0; i < 3; i++) {
        bool minus_zero = false;
        if (!bits_parse(text[i], width, &b[i], err, sizeof err)) {
            return fail(err);
        }
        code_decode(b[i], KIND_UNSIGNED, &uns[i], &minus_zero);
        code_decode(b[i], KIND_COMP, &sig[i], &minus_zero);
    }
    fprintf(out, "【同一机器数的两种读法】字长 %d 位\n", width);
    for (i = 0; i < 3; i++) {
        bits_write_hex(hex, sizeof hex, b[i]);
        fprintf(out, "%s = %s：无符号 %lld，补码 %lld\n", name[i], hex, uns[i], sig[i]);
    }
    format_order(order_u, sizeof order_u, uns);
    format_order(order_s, sizeof order_s, sig);
    fprintf(out, "无符号顺序：%s\n", order_u);
    fprintf(out, "补码顺序：%s\n", order_s);
    fprintf(out, "结果：无符号 %s；补码 %s\n", order_u, order_s);
    return 0;
}
