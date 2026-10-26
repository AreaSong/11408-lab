#include "frac.h"

void frac_set(Frac *f, long long num, long long den) {
    long long a;
    long long b;
    long long t;

    if (den < 0) {
        num = -num;
        den = -den;
    }
    a = num < 0 ? -num : num;
    b = den;
    while (b != 0) {
        t = a % b;
        a = b;
        b = t;
    }
    if (a == 0) {
        a = 1;
    }
    f->num = num / a;
    f->den = den / a;
}
