#ifndef NFP_H
#define NFP_H

/*
 * 非 IEEE 浮点。阶码和尾数都是补码，而且各含 2 位符号位。
 * 2009-13 用的就是这套：没有移码，也没有隐含 1。
 * 尾数的 frac 是补码整数，小数点在双符号位后面，分母是 2^(尾数位数-2)。
 */

enum { NFP_NORM = 0, NFP_RIGHT = 1, NFP_LEFT = 2, NFP_ZERO = 3 };

typedef struct {
    int exp_bits;
    int frac_bits;
    int exp;
    int frac;
    int overflow; /* 1 上溢，-1 下溢，0 这个数本身能表示 */
} NfpNum;

typedef struct {
    NfpNum x;
    NfpNum y;
    int y_small;
    int diff;
    int aligned_frac;
    int align_any;
    int align_show;
    unsigned align_lost;
    int sum_exp;
    int sum_frac;
    int norm_dir; /* 1 右规，-1 左规，0 不用移 */
    int norm_n;
    int norm_lost;
    int result_exp;
    int result_frac;
    int overflow;
} NfpAdd;

int nfp_low(int value, int width);
int nfp_signed(int bits, int width);
int nfp_shape(int frac, int width);

/* 0 成功。1 位数不合法，2 阶码超范围，3 尾数不是二进制小数，4 尾数放不进补码。 */
int nfp_from_fraction(NfpNum *out, int exp_bits, int frac_bits, int exp, int numer, int denom);
int nfp_from_bits(NfpNum *out, int exp_bits, int frac_bits, const char *exp_text, const char *frac_text);
int nfp_add(NfpAdd *out, const NfpNum *a, const NfpNum *b);

#endif
