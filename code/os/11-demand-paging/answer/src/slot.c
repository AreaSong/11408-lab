#include "slot.h"

#include <limits.h>

static int mul_ok(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static int add_ok(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a > ULLONG_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

int addr_shift(unsigned long long page_bytes, int *shift) {
    int n = 0;
    unsigned long long value = page_bytes;

    if (value < 2 || (value & (value - 1)) != 0) {
        return -1;
    }
    while (value > 1) {
        value >>= 1;
        n++;
    }
    *shift = n;
    return 0;
}

int addr_split(unsigned long long page_bytes, unsigned long long addr, unsigned long long *page,
               unsigned long long *offset) {
    int shift = 0;

    if (addr_shift(page_bytes, &shift) != 0) {
        return -1;
    }
    *page = addr >> shift;
    *offset = addr & (page_bytes - 1);
    return 0;
}

/* 缺页处理后要改的那一项：页表始址 + 页号 × 页表项字节数。2024-45 的页表项地址就是这么来的。 */
int pte_addr(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long base,
             unsigned long long page, unsigned long long *addr) {
    unsigned long long bytes = 0;
    int shift = 0;

    if (addr_shift(page_bytes, &shift) != 0 || shift < 1 || pte_bytes == 0) {
        return -1;
    }
    if (mul_ok(page, pte_bytes, &bytes) != 0 || add_ok(base, bytes, addr) != 0) {
        return -1;
    }
    return 0;
}
