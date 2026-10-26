#include "asl.h"

static long long gcd_abs(long long a, long long b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static void positive_den(long long *num, long long *den) {
    if (*den < 0) {
        *num = -*num;
        *den = -*den;
    }
}

void asl_write(FILE *out, long long num, long long den) {
    long long g;
    long long scaled;
    long long dec;
    long long ip;
    long long fp;
    positive_den(&num, &den);
    if (den == 0) {
        fprintf(out, "0/0");
        return;
    }
    g = gcd_abs(num, den);
    num /= g;
    den /= g;
    /* 加半个分母再整除，半数进位，和手算「保留两位」一致。 */
    scaled = num * 100;
    if (num >= 0) {
        scaled += den / 2;
    } else {
        scaled -= den / 2;
    }
    dec = scaled / den;
    ip = dec / 100;
    fp = dec % 100;
    if (fp < 0) {
        fp = -fp;
    }
    if (dec < 0 && ip == 0) {
        fprintf(out, "%lld/%lld=-0.%02lld", num, den, fp);
    } else {
        fprintf(out, "%lld/%lld=%lld.%02lld", num, den, ip, fp);
    }
}

int asl_same(long long n1, long long d1, long long n2, long long d2) {
    positive_den(&n1, &d1);
    positive_den(&n2, &d2);
    if (d1 == 0 || d2 == 0) {
        return 0;
    }
    return n1 * d2 == n2 * d1;
}

int asl_less(long long n1, long long d1, long long n2, long long d2) {
    positive_den(&n1, &d1);
    positive_den(&n2, &d2);
    if (d1 == 0 || d2 == 0) {
        return 0;
    }
    return n1 * d2 < n2 * d1;
}
