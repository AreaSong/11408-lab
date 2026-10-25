#include "radix.h"

#include "number.h"

#include <stdio.h>
#include <string.h>

static const char *DIGITS = "0123456789ABCDEF";

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int digit_of(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int bit_width(int base) {
    if (base == 2) {
        return 1;
    }
    if (base == 8) {
        return 3;
    }
    if (base == 16) {
        return 4;
    }
    return 0;
}

static int collect(const char *text, int base, int *sign, int *id, int *ni, int *fd, int *nf) {
    const char *p = text;
    int frac = 0;
    *sign = 1;
    *ni = 0;
    *nf = 0;
    if (*p == '+' || *p == '-') {
        *sign = (*p == '-') ? -1 : 1;
        p++;
    }
    while (*p != '\0') {
        int d;
        if (*p == '.') {
            frac = 1;
            p++;
            continue;
        }
        d = digit_of(*p);
        if (d < 0 || d >= base || *ni >= 64 || *nf >= 64) {
            return -1;
        }
        if (frac) {
            fd[(*nf)++] = d;
        } else {
            id[(*ni)++] = d;
        }
        p++;
    }
    return 0;
}

static int expand(const int *digs, int n, int width, int *bits) {
    int i;
    int k = 0;
    for (i = 0; i < n; i++) {
        int b;
        for (b = width - 1; b >= 0; b--) {
            bits[k++] = (digs[i] >> b) & 1;
        }
    }
    return k;
}

static int with_pad(const int *src, int n, int group, int on_right, int *dst) {
    int pad = n == 0 ? 0 : (group - (n % group)) % group;
    int i;
    if (!on_right) {
        for (i = 0; i < pad; i++) {
            dst[i] = 0;
        }
        for (i = 0; i < n; i++) {
            dst[pad + i] = src[i];
        }
    } else {
        for (i = 0; i < n; i++) {
            dst[i] = src[i];
        }
        for (i = 0; i < pad; i++) {
            dst[n + i] = 0;
        }
    }
    return n + pad;
}

static int pack(const int *bits, int n, int group, int *digs) {
    int i;
    int k = 0;
    for (i = 0; i + group <= n; i += group) {
        int v = 0;
        int j;
        for (j = 0; j < group; j++) {
            v = (v << 1) | bits[i + j];
        }
        digs[k++] = v;
    }
    return k;
}

static int strip_lead(int *digs, int n) {
    int i = 0;
    while (i + 1 < n && digs[i] == 0) {
        i++;
    }
    if (i > 0) {
        memmove(digs, digs + i, (size_t)(n - i) * sizeof *digs);
    }
    return n - i;
}

static int strip_tail(int *digs, int n) {
    while (n > 0 && digs[n - 1] == 0) {
        n--;
    }
    return n;
}

static void append_digits(char *dst, size_t n, const int *digs, int count) {
    size_t k = strlen(dst);
    int i;
    for (i = 0; i < count && k + 1 < n; i++) {
        dst[k++] = DIGITS[digs[i]];
    }
    dst[k] = '\0';
}

static int grouped(int from, int to, const char *text, char *dst, size_t n) {
    int id[64];
    int fd[64];
    int ibits[320];
    int fbits[320];
    int ipad[320];
    int fpad[320];
    int idig[80];
    int fdig[80];
    int sign = 1;
    int ni = 0;
    int nf = 0;
    int in = 0;
    int fn = 0;
    int src_w = bit_width(from);
    int dst_w = bit_width(to);
    if (collect(text, from, &sign, id, &ni, fd, &nf) != 0 || src_w == 0 || dst_w == 0) {
        return -1;
    }
    in = ni == 0 ? 0 : expand(id, ni, src_w, ibits);
    fn = nf == 0 ? 0 : expand(fd, nf, src_w, fbits);
    if (in == 0) {
        idig[0] = 0;
        in = 1;
    } else {
        in = pack(ipad, with_pad(ibits, in, dst_w, 0, ipad), dst_w, idig);
        in = strip_lead(idig, in);
    }
    fn = nf == 0 ? 0 : strip_tail(fdig, pack(fpad, with_pad(fbits, fn, dst_w, 1, fpad), dst_w, fdig));
    dst[0] = '\0';
    if (sign < 0 && !(in == 1 && idig[0] == 0 && fn == 0)) {
        snprintf(dst, n, "-");
    }
    append_digits(dst, n, idig, in);
    if (fn > 0) {
        strncat(dst, ".", n - strlen(dst) - 1);
        append_digits(dst, n, fdig, fn);
    }
    return 0;
}

static void print_padded(FILE *out, const char *title, const int *digs, int n, int src_w, int dst_w, int on_right) {
    int bits[320];
    int padded[320];
    int i;
    int nb;
    int np;
    if (n == 0) {
        return;
    }
    nb = expand(digs, n, src_w, bits);
    np = with_pad(bits, nb, dst_w, on_right, padded);
    fprintf(out, "%s：", title);
    for (i = 0; i < np; i++) {
        if (i > 0 && dst_w > 1 && i % dst_w == 0) {
            fputc(' ', out);
        }
        fputc(padded[i] ? '1' : '0', out);
    }
    fprintf(out, "\n");
}

static int explain_group(FILE *out, int from, int to, const char *text, const char *expect) {
    char got[160];
    int id[64];
    int fd[64];
    int sign = 1;
    int ni = 0;
    int nf = 0;
    if (grouped(from, to, text, got, sizeof got) != 0) {
        return fail("分组失败");
    }
    if (strcmp(got, expect) != 0) {
        fprintf(stderr, "分组得到 %s，换算得到 %s\n", got, expect);
        return 1;
    }
    collect(text, from, &sign, id, &ni, fd, &nf);
    fprintf(out, "2、8、16 互转不用除法：%d 进制 1 位换成 %d 位二进制，再按 %d 位一组写成 %d 进制\n", from,
            bit_width(from), bit_width(to), to);
    print_padded(out, "整数", id, ni, bit_width(from), bit_width(to), 0);
    print_padded(out, "小数", fd, nf, bit_width(from), bit_width(to), 1);
    fprintf(out, "整数向左补 0，小数向右补 0。小数末尾的 0 不改变数值，结果里去掉\n");
    fprintf(out, "分组结果 %s\n", got);
    return 0;
}

static void print_div(FILE *out, unsigned long long x, int base) {
    fprintf(out, "整数部分 %llu，除 %d 取余，先得到的余数是低位：\n", x, base);
    if (x == 0) {
        fprintf(out, "0 ÷ %d = 0 余 0\n", base);
        return;
    }
    while (x > 0) {
        unsigned long long q = x / (unsigned)base;
        int r = (int)(x % (unsigned)base);
        if (r >= 10) {
            fprintf(out, "%llu ÷ %d = %llu 余 %d（%c）\n", x, base, q, r, DIGITS[r]);
        } else {
            fprintf(out, "%llu ÷ %d = %llu 余 %d\n", x, base, q, r);
        }
        x = q;
    }
    fprintf(out, "余数从下往上读\n");
}

static int print_mul(FILE *out, unsigned long long fn, unsigned long long fd, int base, int max_digits) {
    char cur[64];
    char rest[64];
    int n = 0;
    format_ratio(fn, fd, cur, sizeof cur);
    fprintf(out, "小数部分 %s，乘 %d 取整，先得到的是高位，最多 %d 位：\n", cur, base, max_digits);
    while (fn != 0 && n < max_digits) {
        unsigned long long prod;
        int digit;
        if (fn > (~0ULL / (unsigned)base)) {
            return fail("小数相乘放不进 64 位");
        }
        prod = fn * (unsigned)base;
        digit = (int)(prod / fd);
        fn = prod % fd;
        format_ratio(fn, fd, rest, sizeof rest);
        fprintf(out, "%s × %d 取 %c，余 %s\n", cur, base, DIGITS[digit], rest);
        snprintf(cur, sizeof cur, "%s", rest);
        n++;
    }
    if (fn == 0) {
        fprintf(out, "正好除尽\n");
    } else {
        fprintf(out, "还没除尽，只保留前 %d 位\n", max_digits);
    }
    return 0;
}

static void print_int_weights(FILE *out, const int *digs, int n, int base) {
    unsigned long long power = 1;
    unsigned long long sum = 0;
    int i;
    int exp;
    if (n == 0) {
        fprintf(out, "整数部分 = 0\n");
        return;
    }
    for (exp = 0; exp < n - 1; exp++) {
        power *= (unsigned)base;
    }
    for (i = 0; i < n; i++) {
        unsigned long long term = (unsigned)digs[i] * power;
        fprintf(out, "%c × %d^%d = %llu\n", DIGITS[digs[i]], base, n - 1 - i, term);
        sum += term;
        if (i + 1 < n) {
            power /= (unsigned)base;
        }
    }
    fprintf(out, "整数部分 = %llu\n", sum);
}

static void print_frac_weights(FILE *out, const int *digs, int n, int base) {
    unsigned long long den = 1;
    int k;
    for (k = 0; k < n; k++) {
        char shown[64];
        den *= (unsigned)base;
        format_ratio((unsigned)digs[k], den, shown, sizeof shown);
        fprintf(out, "%c × %d^-%d = %s\n", DIGITS[digs[k]], base, k + 1, shown);
    }
}

static int print_weights(FILE *out, int base, const char *text) {
    int id[64];
    int fd[64];
    int sign = 1;
    int ni = 0;
    int nf = 0;
    if (collect(text, base, &sign, id, &ni, fd, &nf) != 0) {
        return fail("无法按权展开");
    }
    fprintf(out, "【按权展开】某一位的权是底数的整数次幂，小数点右边从 -1 开始\n");
    print_int_weights(out, id, ni, base);
    if (nf > 0) {
        print_frac_weights(out, fd, nf, base);
    }
    return 0;
}

int radix_print(FILE *out, int from, int to, int frac_digits, const char *text) {
    RadixParts part;
    char result[160];
    char err[160];
    bool exact = false;
    int from_w = bit_width(from);
    int to_w = bit_width(to);
    if (!radix_base_ok(from) || !radix_base_ok(to)) {
        return fail("进制只能是 2、8、10、16");
    }
    if (radix_result(from, to, frac_digits, text, result, sizeof result, &exact, err, sizeof err) != 0) {
        return fail(err);
    }
    if (radix_parse(from, text, &part, err, sizeof err) != 0) {
        return fail(err);
    }
    fprintf(out, "【%s转%s】%s\n", radix_name(from), radix_name(to), text);
    if (from_w != 0 && to_w != 0) {
        if (explain_group(out, from, to, text, result) != 0) {
            return 1;
        }
    } else if (to == 10 && from != 10) {
        if (print_weights(out, from, text) != 0) {
            return 1;
        }
    } else if (from == to) {
        fprintf(out, "源和目标进制相同，只把写法规范一下\n");
    } else {
        print_div(out, part.ip, to);
        if (part.fn != 0 && print_mul(out, part.fn, part.fd, to, frac_digits) != 0) {
            return 1;
        }
    }
    if (!exact) {
        fprintf(out, "小数是循环的或被截断了，结果只保留指定的位数\n");
    }
    if (part.sign < 0) {
        fprintf(out, "负号单独保留。这里转的是绝对值，不是补码\n");
    }
    fprintf(out, "结果：%s\n", result);
    return 0;
}
