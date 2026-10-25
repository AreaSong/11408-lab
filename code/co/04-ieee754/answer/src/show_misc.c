#include "show.h"
#include "show_util.h"

#include <stdint.h>

int show_fields(FILE *out) {
    char exam[80];
    char hex[20];
    View min_norm = fp_view(0x00800000u, &FMT32);
    View min_sub = fp_view(0x00000001u, &FMT32);
    uint64_t min_double = 1ULL << 52;
    fp_exam(&min_norm, &FMT32, exam, sizeof exam);
    fp_hex(0x00800000u, 8, hex);
    fprintf(out, "【字段划分】\n");
    fprintf(out, "单精度 float：1 位符号 + 8 位阶码 + 23 位尾数，共 32 位\n");
    fprintf(out, "阶码是移码，偏置 127。全 0 和全 1 留给特殊值，所以偏置不是普通 8 位移码的 128\n");
    fprintf(out, "规格化真阶从 -126 到 127。尾数最高位的 1 不存\n");
    fprintf(out, "最小正规格化数：阶码取 1，尾数全 0，真值 %s，机器数 %s\n", exam, hex);
    fprintf(out, "最大规格化数：阶码 254，尾数全 1，真值 (2-2^(-23))×2^127，机器数 7F7FFFFFH\n");
    fprintf(out, "+0 是 00000000H，-0 是 80000000H，正无穷 7F800000H，负无穷 FF800000H\n");
    fp_exam(&min_sub, &FMT32, exam, sizeof exam);
    fprintf(out, "最小正非规格化数真值 %s，机器数 00000001H\n", exam);
    fprintf(out, "双精度 double：1 位符号 + 11 位阶码 + 52 位尾数，共 64 位，偏置 1023\n");
    fp_hex(min_double, 16, hex);
    fprintf(out, "规格化真阶从 -1022 到 1023。最小正规格化数 1.0×2^(-1022)，机器数 %s\n", hex);
    fprintf(out, "双精度有效数字 53 位，32 位整数都能精确放进去\n");
    fprintf(out, "结果：1.0×2^(-126)\n");
    return 0;
}

static int prefer_int(int k) {
    return k <= 30;
}

static int prefer_float(int k) {
    return k <= 24;
}

static void one_range(FILE *out, const char *name, int k) {
    fprintf(out, "%s：-2^%d 到 2^%d 里的每个整数都要精确\n", name, k, k);
    fprintf(out, "32 位补码是 -2^31 到 2^31-1，正的 2^%d %s放下\n", k, prefer_int(k) ? "能" : "不能");
    fprintf(out, "float 有效数字 24 位，%s覆盖这个区间\n", prefer_float(k) ? "能" : "不能");
    if (!prefer_float(k) && k <= 127) {
        fprintf(out, "2^%d 本身是 2 的幂，float 能表示它，但区间里别的整数会丢\n", k);
    }
    fprintf(out, "double 有效数字 53 位，%s覆盖\n", k <= 53 ? "能" : "不能");
    if (prefer_int(k)) {
        fprintf(out, "整数运算更快，%s 用 32 位整数\n", name);
    } else if (k <= 53) {
        fprintf(out, "%s 用双精度浮点数\n", name);
    } else {
        fprintf(out, "%s 连双精度也盖不住全部整数\n", name);
    }
}

static const char *choice_of(int k) {
    if (prefer_int(k)) {
        return "32 位整数";
    }
    if (k <= 53) {
        return "双精度浮点数";
    }
    return "超出双精度";
}

int show_fit(FILE *out, int ka, int kb) {
    if (ka < 0 || kb < 0 || ka > 200 || kb > 200) {
        fprintf(stderr, "指数请落在 0 到 200\n");
        return 1;
    }
    fprintf(out, "【整数该用哪种表示】α 的范围 2^%d，β 的范围 2^%d\n", ka, kb);
    one_range(out, "α", ka);
    one_range(out, "β", kb);
    fprintf(out, "结果：α 用 %s，β 用 %s\n", choice_of(ka), choice_of(kb));
    return 0;
}

static int integer_mag(const Decimal *dec, uint64_t *mag) {
    uint64_t value = dec->coef;
    int exp10 = dec->exp10;
    if (exp10 < 0) {
        return 0;
    }
    while (exp10 > 0) {
        if (value > UINT64_MAX / 10) {
            return 0;
        }
        value *= 10;
        exp10--;
    }
    *mag = value;
    return 1;
}

static int trunc_int32(const View *view, int *out) {
    uint64_t sig;
    int shift;
    uint64_t ip;
    if (view->kind == KIND_NAN || view->kind == KIND_INF || view->exp_true > 31) {
        return 0;
    }
    if (view->kind == KIND_ZERO || view->exp_true < 0) {
        *out = 0;
        return 1;
    }
    sig = view->sig;
    shift = view->exp_true - 23;
    ip = (shift >= 0) ? (sig << (unsigned)shift) : (sig >> (unsigned)(-shift));
    if (ip > 2147483648ULL || (ip == 2147483648ULL && !view->sign)) {
        return 0;
    }
    if (ip == 2147483648ULL) {
        *out = -2147483647 - 1;
        return 1;
    }
    *out = view->sign ? -(int)ip : (int)ip;
    return 1;
}

static void cast_result(FILE *out, int is_int, int fit32, int float_bad, int double_bad, int trunc_ok,
                        int trunc) {
    if (is_int && !fit32) {
        fprintf(out, "结果：放不进 32 位 int%s\n", float_bad ? "，float 也会丢精度" : "，float 能精确表示");
        return;
    }
    if (is_int && float_bad) {
        fprintf(out, "结果：float 丢精度，double %s\n", double_bad ? "也丢精度" : "不丢");
        return;
    }
    if (is_int) {
        fprintf(out, "结果：int 和 float 都精确，double 也精确\n");
        return;
    }
    if (trunc_ok) {
        fprintf(out, "结果：向零截断成 %d\n", trunc);
        return;
    }
    fprintf(out, "结果：放不进 32 位 int\n");
}

int show_cast(FILE *out, const char *text) {
    Decimal dec;
    EncShow single;
    EncShow wide;
    View view;
    char err[160];
    char fhex[16];
    char dhex[20];
    char exact[80];
    uint64_t mag = 0;
    int is_int;
    int fit32 = 0;
    int trunc = 0;
    int trunc_ok;
    if (!dec_parse(text, &dec, err, sizeof err) || !fp_encode(&dec, &FMT32, &single, err, sizeof err) ||
        !fp_encode(&dec, &FMT64, &wide, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    view = fp_view(single.bits, &FMT32);
    fp_hex(single.bits, 8, fhex);
    fp_hex(wide.bits, 16, dhex);
    value_text((uint32_t)single.bits, exact, sizeof exact);
    is_int = integer_mag(&dec, &mag);
    fprintf(out, "【类型转换】%s\n", text);
    if (is_int) {
        fit32 = mag < 2147483648ULL || (mag == 2147483648ULL && dec.sign);
        fprintf(out, "这是整数%s\n", fit32 ? "，放得进 32 位 int" : "，放不进 32 位 int");
    }
    fprintf(out, "float 机器数 %s，真值 %s。%s\n", fhex, exact, single.inexact ? "有舍入" : "没有舍入");
    fprintf(out, "double 机器数 %s，%s\n", dhex, wide.inexact ? "也有舍入" : "没有舍入");
    fprintf(out, "float 或 double 转 int 时丢掉小数，向零截断。-1.9 得到 -1，不是 -2\n");
    trunc_ok = trunc_int32(&view, &trunc);
    if (trunc_ok) {
        fprintf(out, "这个 float 向零截断得到 %d\n", trunc);
    } else {
        fprintf(out, "这个 float 的幅度放不进 32 位 int，C 语言里这种转换没有定义，这里不转\n");
    }
    cast_result(out, is_int, fit32, single.inexact, wide.inexact, trunc_ok, trunc);
    return 0;
}

static int f1_bits(unsigned n, uint32_t *bits, int *as_int) {
    uint64_t sum = 1;
    uint64_t power = 1;
    unsigned i;
    if (n > 62) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        power *= 2;
        sum += power;
    }
    *bits = (uint32_t)sum;
    *as_int = (int)(int32_t)(*bits);
    return 1;
}

static uint32_t f2_bits(unsigned n) {
    float sum = 1.0f;
    float power = 1.0f;
    unsigned i;
    union {
        float f;
        uint32_t u;
    } box;
    for (i = 0; i < n; i++) {
        power *= 2.0f;
        sum += power;
    }
    box.f = sum;
    return box.u;
}

static void sum_zero(FILE *out) {
    fprintf(out, "n 是 unsigned，n-1 的机器数是 32 个 1，值是 2^32-1\n");
    fprintf(out, "i 也是 unsigned，i <= n-1 永远成立，循环停不下来\n");
    fprintf(out, "若 i 和 n 都改成 int，n-1 = -1。i 从 0 开始，0 <= -1 不成立，循环一次都不进\n");
    fprintf(out, "结果：死循环；i 和 n 改成 int 后不会\n");
}

static void sum_result(FILE *out, unsigned n, int has_f1, int f1, uint32_t f1b, uint32_t f2b) {
    char h1[16];
    char h2[16];
    View f2 = fp_view(f2b, &FMT32);
    fp_hex(f1b, 8, h1);
    fp_hex(f2b, 8, h2);
    if (n == 23 && has_f1 && f1 == 16777215 && f2b == 0x4B7FFFFFu) {
        fprintf(out, "24 个 1。int 放得下。float 的有效数字也是 24 位，不用舍入\n");
        fprintf(out, "结果：相等，f1 机器数 %s，f2 机器数 %s\n", h1, h2);
        return;
    }
    if (n == 24 && has_f1) {
        fprintf(out, "25 个 1。float 只留 24 位，向偶数舍入后变成 2^25，比 f1 大 1\n");
        fprintf(out, "结果：f1 = %d，f2 = 33554432\n", f1);
        return;
    }
    if (n == 31 && has_f1) {
        fprintf(out, "2^32-1 的低 32 位是 32 个 1，按 int 读是 -1。int 能表示的最大正数是 2^31-1，对应 n = 30\n");
        fprintf(out, "结果：f1 = %d，和 f(n) 相等的最大 n 是 30\n", f1);
        return;
    }
    if (f2.kind == KIND_INF) {
        fprintf(out, "阶码全 1、尾数全 0，这是正无穷。2^128 超出单精度\n");
        fprintf(out, "不溢出的最大 n 是 126，那时结果是 2^127，阶码 254\n");
        fprintf(out, "不舍入的最大 n 是 23，2^24-1 正好占满 24 位有效数字\n");
        fprintf(out, "结果：正无穷，机器数 %s；不溢出最大 n = 126，不舍入最大 n = 23\n", h2);
        return;
    }
    if (has_f1) {
        fprintf(out, "结果：f1 = %d，机器数 %s；f2 机器数 %s\n", f1, h1, h2);
        return;
    }
    fprintf(out, "结果：f2 机器数 %s\n", h2);
}

int show_sum(FILE *out, unsigned n) {
    uint32_t f1b = 0;
    uint32_t f2b;
    int f1 = 0;
    int has_f1;
    char h2[16];
    char text[80];
    if (n > 200) {
        fprintf(stderr, "n 请落在 0 到 200\n");
        return 1;
    }
    fprintf(out, "【f1 与 f2】n = %u\n", n);
    fprintf(out, "f(n) = 2^(n+1)-1，二进制是 %u 个 1\n", n + 1);
    if (n == 0) {
        sum_zero(out);
        return 0;
    }
    has_f1 = f1_bits(n, &f1b, &f1);
    f2b = f2_bits(n);
    if (has_f1) {
        char h1[16];
        fp_hex(f1b, 8, h1);
        fprintf(out, "f1 按 32 位补码回绕后 = %d，机器数 %s\n", f1, h1);
    } else {
        fprintf(out, "f1 的真和已经超过 64 位，这里不模拟它的回绕\n");
    }
    fp_hex(f2b, 8, h2);
    value_text(f2b, text, sizeof text);
    fprintf(out, "f2 的 float 真值 %s，机器数 %s\n", text, h2);
    sum_result(out, n, has_f1, f1, f1b, f2b);
    return 0;
}
