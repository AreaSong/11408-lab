#include "alu.h"
#include "cla.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void views(char *dst, size_t n, Word w) {
    char hex[32];
    char bin[40];
    word_write_hex(hex, sizeof hex, w);
    word_write_bin(bin, sizeof bin, w);
    snprintf(dst, n, "%s（%s，无符号 %llu，补码 %lld）", hex, bin, word_unsigned(w), word_signed(w));
}

int fa_print(FILE *out, int a, int b, int cin) {
    FullAdd bit;
    int xy;
    if ((a != 0 && a != 1) || (b != 0 && b != 1) || (cin != 0 && cin != 1)) {
        return fail("fa 的三个输入只能是 0 或 1");
    }
    bit = full_add(a, b, cin);
    xy = (a & 1) ^ (b & 1);
    fprintf(out, "【一位全加器】a=%d，b=%d，低位进位 c=%d\n", a, b, cin);
    fprintf(out, "本位和 s = a ⊕ b ⊕ c = %d ⊕ %d ⊕ %d = %d\n", a, b, cin, bit.sum);
    fprintf(out, "进位 cout = (a ∧ b) ∨ (c ∧ (a ⊕ b)) = (%d ∧ %d) ∨ (%d ∧ %d) = %d\n", a, b, cin, xy,
            bit.cout);
    fprintf(out, "结果：和 %d，进位 %d\n", bit.sum, bit.cout);
    return 0;
}

static void print_ripple(FILE *out, const AluResult *r) {
    int i;
    fprintf(out, "位 0 是最低位。进位从这里逐级送到最高位，所以叫串行进位\n");
    fprintf(out, "不算 G、P 的形成时间。一级与门或一级或门算 1 ty，每位进位 2 ty，%d 位最长 %d ty\n", r->a.width,
            r->a.width * 2);
    for (i = 0; i < r->a.width; i++) {
        int sum = (int)((r->ripple.sum >> i) & 1ULL);
        fprintf(out, "位 %d：%d ⊕ %d ⊕ 进位 %d → 本位和 %d，向高位进位 %d\n", i, word_bit(r->a, i),
                word_bit(r->rhs, i), r->ripple.carry[i], sum, r->ripple.carry[i + 1]);
    }
}

static void print_flags(FILE *out, const AluResult *r) {
    int width = r->a.width;
    fprintf(out, "最高位（位 %d）产生的进位 Cout = %d\n", width - 1, r->c_out);
    fprintf(out, "次高位（位 %d）产生的进位 = %d\n", width - 2, r->c_into);
    fprintf(out, "OF = Cout ⊕ 次高位进位 = %d ⊕ %d = %d。OF 只表示有符号溢出\n", r->c_out, r->c_into, r->of);
    if (r->sub) {
        fprintf(out, "CF = Cout ⊕ Sub = %d ⊕ 1 = %d。这次是减法，CF 是借位，和 Cout 相反\n", r->c_out, r->cf);
    } else {
        fprintf(out, "CF = Cout ⊕ Sub = %d ⊕ 0 = %d。这次是加法，Sub=0，CF 就是最高位进位\n", r->c_out, r->cf);
    }
    fprintf(out, "ZF = %d，结果%s全 0。SF = %d，等于结果最高位；溢出时真值符号看双符号位的左边那位\n", r->zf,
            r->zf ? "是" : "不是", r->sf);
}

static void print_wide(FILE *out, const AluResult *r) {
    char bin[48];
    word_write_bin(bin, sizeof bin, r->wide);
    fprintf(out, "双符号位先把符号再复制一位。存回寄存器时仍然只有一位符号\n");
    if (r->sub) {
        fprintf(out, "减法要先复制符号，再对这个更宽的数取负，不能先在原字长里取负\n");
    }
    fprintf(out, "扩展后的和 %s，两个符号位是 %d%d", bin, r->sign_hi, r->sign_lo);
    if (r->sign_hi == r->sign_lo) {
        fprintf(out, "，相同，无溢出\n");
    } else if (r->sign_hi == 0) {
        fprintf(out, "，正溢\n");
    } else {
        fprintf(out, "，负溢\n");
    }
}

static void print_result(FILE *out, const AluResult *r) {
    char hex[32];
    word_write_hex(hex, sizeof hex, r->result);
    fprintf(out, "结果：%s，无符号 %llu，补码 %lld，ZF=%d，SF=%d，CF=%d，OF=%d", hex, word_unsigned(r->result),
            word_signed(r->result), r->zf, r->sf, r->cf, r->of);
    if (r->of) {
        fprintf(out, "，有符号真值 %lld 溢出", r->math_signed);
    }
    fprintf(out, "\n");
}

static void print_head(FILE *out, const AluResult *r) {
    char left[160];
    char right[160];
    views(left, sizeof left, r->a);
    views(right, sizeof right, r->b);
    if (r->sub) {
        char rhs[160];
        views(rhs, sizeof rhs, r->rhs);
        fprintf(out, "【%d 位减法】a-b = a+(~b)+1，模 2^%d。有符号和无符号走同一条加法器\n", r->a.width,
                r->a.width);
        fprintf(out, "a = %s\nb = %s\n~b = %s\n", left, right, rhs);
        fprintf(out, "最低进位 c0 = 1，Sub = 1\n");
        return;
    }
    fprintf(out, "【%d 位加法】模 2^%d，Sub = 0。有符号和无符号走同一条加法器\n", r->a.width, r->a.width);
    fprintf(out, "a = %s\nb = %s\n", left, right);
    fprintf(out, "最低进位 c0 = 0\n");
}

static int print_logic(FILE *out, const AluResult *r) {
    char left[160];
    char right[160];
    char hex[32];
    views(left, sizeof left, r->a);
    views(right, sizeof right, r->b);
    word_write_hex(hex, sizeof hex, r->result);
    fprintf(out, "【%d 位%s】按位逻辑，没有进位链\n", r->a.width, r->op == ALU_AND ? "与" : "或");
    fprintf(out, "a = %s\nb = %s\n", left, right);
    fprintf(out, "与、或不产生 CF 和 OF。x86 会另置 ZF、SF 并把 CF、OF 清 0；这里只取位结果\n");
    fprintf(out, "结果：%s\n", hex);
    return 0;
}

int alu_print(FILE *out, const AluResult *r) {
    if (!r->has_flags) {
        return print_logic(out, r);
    }
    print_head(out, r);
    print_ripple(out, r);
    print_flags(out, r);
    print_wide(out, r);
    print_result(out, r);
    return 0;
}

static void print_cla_formula(FILE *out, const Cla4 *c) {
    fprintf(out, "c1 = G0 ∨ (P0 ∧ c0) = %d\n", c->c[1]);
    fprintf(out, "c2 = G1 ∨ (P1 ∧ G0) ∨ (P1 ∧ P0 ∧ c0) = %d\n", c->c[2]);
    fprintf(out, "c3 = G2 ∨ (P2 ∧ G1) ∨ (P2 ∧ P1 ∧ G0) ∨ (P2 ∧ P1 ∧ P0 ∧ c0) = %d\n", c->c[3]);
    fprintf(out, "c4 = G3 ∨ (P3 ∧ G2) ∨ (P3 ∧ P2 ∧ G1) ∨ (P3 ∧ P2 ∧ P1 ∧ G0) ∨ (P3 ∧ P2 ∧ P1 ∧ P0 ∧ c0) = %d\n",
            c->c[4]);
}

int cla_print(FILE *out, Word a, Word b) {
    Cla4 c;
    Word sum;
    char abin[8];
    char bbin[8];
    char sbin[8];
    char hex[16];
    int i;
    if (a.width != 4 || b.width != 4) {
        return fail("cla 只做 4 位。位数再多，与项会很长，实际要分组");
    }
    c = cla4(a, b, 0);
    sum = word_make(4, c.sum);
    word_write_bin(abin, sizeof abin, a);
    word_write_bin(bbin, sizeof bbin, b);
    word_write_bin(sbin, sizeof sbin, sum);
    word_write_hex(hex, sizeof hex, sum);
    fprintf(out, "【4 位先行进位】a=%s，b=%s，最低进位 c0=0\n", abin, bbin);
    fprintf(out, "产生 G_i = A_i ∧ B_i，传递 P_i = A_i ⊕ B_i\n");
    fprintf(out, "G 从高位到低位 %d%d%d%d，P 从高位到低位 %d%d%d%d\n", c.g[3], c.g[2], c.g[1], c.g[0], c.p[3],
            c.p[2], c.p[1], c.p[0]);
    for (i = 0; i < 4; i++) {
        fprintf(out, "位 %d：G=%d，P=%d，本位和 P⊕c = %d\n", i, c.g[i], c.p[i], c.p[i] ^ c.c[i]);
    }
    fprintf(out, "串行进位要等低位算完。一级门算 1 ty，不算 G、P 的形成时间，4 位从 c0 到 c4 最长 8 ty\n");
    fprintf(out, "先行进位把进位展开，c1 到 c4 同时得到，最长 2 ty\n");
    print_cla_formula(out, &c);
    fprintf(out, "和从高位到低位 %s\n", sbin);
    fprintf(out, "每位一个全加器、进位逐级走，这是串行进位。全机只留一个全加器、数据逐位送，那是串行加法器\n");
    fprintf(out, "结果：和 %s，串行进位 8 ty，先行进位 2 ty\n", hex);
    return 0;
}

int cond_print(FILE *out, const char *name) {
    if (strcmp(name, "bgt") == 0) {
        fprintf(out, "【无符号大于转移 bgt】比较就是做减法\n");
        fprintf(out, "A > B 时没有借位，差也不是 0，所以 CF=0 且 ZF=0\n");
        fprintf(out, "SF 和 OF 是有符号比较用的，无符号大于用不到\n");
        fprintf(out, "选项里的加号是或。¬(CF+ZF)=1 就是 CF、ZF 都为 0\n");
        fprintf(out, "结果：C\n");
        return 0;
    }
    if (strcmp(name, "ule") == 0) {
        fprintf(out, "【无符号小于等于时转移】比较做的是 x-y\n");
        fprintf(out, "差为 0 则 ZF=1；x 比 y 小则有借位，CF=1。这两个有一个成立就该转移\n");
        fprintf(out, "C、Z、N 是检测位，不是标志本身。无符号比较不检测符号标志 N\n");
        fprintf(out, "结果：C=1，Z=1，N=0\n");
        return 0;
    }
    return fail("cond 只接受 bgt 或 ule");
}
