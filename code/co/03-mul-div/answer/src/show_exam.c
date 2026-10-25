#include "exam.h"

#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static void write_wide_hex(char *dst, size_t n, unsigned long long bits, int width) {
    static const char *digits = "0123456789ABCDEF";
    int count = (width + 3) / 4;
    int i;
    size_t k = 0;
    if (n == 0) {
        return;
    }
    for (i = count - 1; i >= 0; i--) {
        int space = ((i + 1) % 4 == 0) && i != count - 1;
        if (k + (size_t)space + 3 >= n) {
            dst[0] = '\0';
            return;
        }
        if (space) {
            dst[k++] = ' ';
        }
        dst[k++] = digits[(int)((bits >> (i * 4)) & 0xF)];
    }
    dst[k++] = 'H';
    dst[k] = '\0';
}

static void print_prod_over(FILE *out, Word x, const Prod *p) {
    Word low = word_make(x.width, p->uprod);
    Word slow = word_make(x.width, p->sprod);
    char ulow[32];
    char slowhex[32];
    word_write_hex(ulow, sizeof ulow, low);
    word_write_hex(slowhex, sizeof slowhex, slow);
    fprintf(out, "只取低 %d 位时，无符号机器数是 %s，补码机器数是 %s\n", x.width, ulow, slowhex);
    fprintf(out, "无符号读低位是 %llu。高 %d 位%s全 0，umul %s\n", word_unsigned(low), x.width,
            p->u_over ? "不是" : "是", p->u_over ? "溢出" : "不溢出");
    fprintf(out, "补码读低位是 %lld。高位和低位的符号扩展%s，imul %s\n", word_signed(slow),
            p->s_over ? "不一致" : "一致", p->s_over ? "溢出" : "不溢出");
    fprintf(out, "无符号截断时，高 n 位全 0 才不溢出\n");
}

int prod_print(FILE *out, Word x, Word y) {
    Prod p;
    char xh[32];
    char yh[32];
    char uh[48];
    char sh[48];
    if (x.width != y.width) {
        return fail("两个整数的位宽要相同");
    }
    prod_eval(x, y, &p);
    word_write_hex(xh, sizeof xh, x);
    word_write_hex(yh, sizeof yh, y);
    write_wide_hex(uh, sizeof uh, p.uprod, x.width * 2);
    write_wide_hex(sh, sizeof sh, p.sprod, x.width * 2);
    fprintf(out, "【n 位整数的 2n 位乘积】n = %d\n", x.width);
    fprintf(out, "x = %s，无符号 %llu，补码 %lld\n", xh, word_unsigned(x), word_signed(x));
    fprintf(out, "y = %s，无符号 %llu，补码 %lld\n", yh, word_unsigned(y), word_signed(y));
    fprintf(out, "无符号乘积的 2n 位是 %s\n", uh);
    fprintf(out, "补码乘积的 2n 位是 %s\n", sh);
    if (p.uprod == p.sprod) {
        fprintf(out, "两份位模式相同\n");
    }
    print_prod_over(out, x, &p);
    fprintf(out, "结果：无符号 %s，补码 %s，umul %s，imul %s\n", uh, sh, p.u_over ? "溢出" : "不溢出",
            p.s_over ? "溢出" : "不溢出");
    return 0;
}

static int impl_speed(FILE *out) {
    fprintf(out, "【乘法的三种做法】\n");
    fprintf(out, "没有乘法指令时，编译器把乘法改成循环，循环里用加法、减法和移位\n");
    fprintf(out, "每条指令都要取指、译码、执行，所以这种最慢\n");
    fprintf(out, "乘法指令若用 ALU 和移位器：控制逻辑管循环次数，并按当前乘数位决定加不加、再移位\n");
    fprintf(out, "这是一条指令，但要多个时钟周期\n");
    fprintf(out, "阵列乘法器把各位部分积同时做出来再相加，没有逐位循环，一个时钟周期就能完成，所以最快\n");
    fprintf(out, "结果：没有乘法指令最慢，阵列乘法器最快\n");
    return 0;
}

static int impl_choice(FILE *out) {
    fprintf(out, "【整数乘法的四种说法】问哪一句是错的\n");
    fprintf(out, "A. 阵列乘法器可以在一个时钟周期内完成。这句对，它是组合逻辑\n");
    fprintf(out, "B. ALU 加移位器做不到一个时钟周期。这句对，它要逐位循环\n");
    fprintf(out, "C. 变量乘常数可以编译成几条移位和加减。这句对，例如乘 10 是左移 3 位再加上左移 1 位\n");
    fprintf(out, "D. 两个变量的乘法不能编译成移位和加法的循环。这句错，没有乘法指令时就是这样做的\n");
    fprintf(out, "结果：D\n");
    return 0;
}

int impl_print(FILE *out, const char *which) {
    if (strcmp(which, "speed") == 0) {
        return impl_speed(out);
    }
    if (strcmp(which, "choice") == 0) {
        return impl_choice(out);
    }
    return fail("impl 需要 speed 或 choice");
}

static void print_cdiv_trap(FILE *out) {
    fprintf(out, "32 位补码除法有两种异常，和上面这一组具体的数无关\n");
    fprintf(out, "除数是 0：x = 0000 0000H，d[i] 可以是任意 32 位机器数\n");
    fprintf(out, "商溢出：d[i] = 8000 0000H，这是 -2^31；x = FFFF FFFFH，这是 -1\n");
    fprintf(out, "(-2^31) / (-1) = 2^31，32 位补码最大只到 2^31-1\n");
    fprintf(out, "响应时：关中断，进入内核态，保存断点（PC 和 PSW），跳到异常处理程序\n");
}

int cdiv_print(FILE *out, Word dividend, Word divisor) {
    char dh[32];
    char xh[32];
    char rh[32];
    char qh[32];
    char yh[32];
    Word r = word_make(32, cdiv_r(dividend));
    Word q = word_make(32, cdiv_q(dividend));
    Word y = word_make(32, cdiv_y(divisor));
    if (dividend.width != 32 || divisor.width != 32) {
        return fail("cdiv 按 32 位字长");
    }
    word_write_hex(dh, sizeof dh, dividend);
    word_write_hex(xh, sizeof xh, divisor);
    word_write_hex(rh, sizeof rh, r);
    word_write_hex(qh, sizeof qh, q);
    word_write_hex(yh, sizeof yh, y);
    fprintf(out, "【补码除法器】字长 32 位。被除数 d[i] = %s，除数 x = %s\n", dh, xh);
    fprintf(out, "d[i] 按补码读是 %lld，x 按补码读是 %lld\n", word_signed(dividend), word_signed(divisor));
    fprintf(out, "scov 把 d[i] 按符号扩展成 64 位。高 32 位放进 R，低 32 位放进 Q\n");
    fprintf(out, "x 是 int。写成 0xff 时值是 255，Y 装这 32 位除数本身\n");
    fprintf(out, "R = %s\n", rh);
    fprintf(out, "Q = %s\n", qh);
    fprintf(out, "Y = %s\n", yh);
    fprintf(out, "计数器在控制逻辑里，用来数 32 轮\n");
    fprintf(out, "加减交替的每一轮，ALU 只做加法或减法。左移由移位器完成，不在 ALUop 里\n");
    print_cdiv_trap(out);
    fprintf(out, "结果：R = %s，Q = %s，Y = %s，计数器在控制逻辑，ALUop 是加法和减法\n", rh, qh, yh);
    return 0;
}
