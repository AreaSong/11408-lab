#include "addr.h"

#include "io.h"

int addr_cut(const int *widths, int n, unsigned long long addr, unsigned long long *parts) {
    int sum = 0;
    int i;
    unsigned long long rest;

    for (i = 0; i < n; i++) {
        if (widths[i] < 1 || widths[i] > 63) {
            return -1;
        }
        sum += widths[i];
    }
    if (sum < 1 || sum > 64) {
        return -1;
    }
    /* 移位 64 位是未定义行为，地址刚好占满 64 位时不再做范围比较。 */
    if (sum < 64 && addr >= (1ULL << sum)) {
        return -2;
    }
    rest = addr;
    for (i = 0; i < n; i++) {
        int low = sum - widths[i];
        parts[i] = (low == 0) ? rest : (rest >> low);
        rest = (low == 0) ? 0 : (rest & ((1ULL << low) - 1));
        sum = low;
    }
    return 0;
}

int addr_pack(const int *widths, const unsigned long long *values, int n, unsigned long long *addr) {
    unsigned long long acc = 0;
    int used = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (widths[i] < 1 || widths[i] > 63 || used + widths[i] > 64) {
            return -1;
        }
        if (values[i] >= (1ULL << widths[i])) {
            return -2;
        }
        acc = (acc << widths[i]) | values[i];
        used += widths[i];
    }
    *addr = acc;
    return 0;
}

int addr_slot(unsigned long long pte_bytes, unsigned long long base, unsigned long long index,
              unsigned long long *out) {
    unsigned long long delta;

    if (pte_bytes == 0 || mul_ull(index, pte_bytes, &delta) != 0 || add_ull(base, delta, out) != 0) {
        return -1;
    }
    return 0;
}

int addr_join(unsigned long long page_bytes, unsigned long long frame, unsigned long long offset,
              unsigned long long *pa) {
    unsigned long long base;

    if (page_bytes == 0 || offset >= page_bytes) {
        return -1;
    }
    if (mul_ull(frame, page_bytes, &base) != 0 || add_ull(base, offset, pa) != 0) {
        return -1;
    }
    return 0;
}

int addr_elem(unsigned long long start, unsigned long long cols, unsigned long long elem_bytes,
              unsigned long long row, unsigned long long col, unsigned long long *index,
              unsigned long long *addr) {
    unsigned long long cells;
    unsigned long long bytes;

    if (cols == 0 || elem_bytes == 0 || col >= cols) {
        return -1;
    }
    if (mul_ull(row, cols, &cells) != 0 || add_ull(cells, col, index) != 0) {
        return -1;
    }
    if (mul_ull(*index, elem_bytes, &bytes) != 0 || add_ull(start, bytes, addr) != 0) {
        return -1;
    }
    return 0;
}

int span_count(unsigned long long page_bytes, unsigned long long length, int *count) {
    unsigned long long n;
    unsigned long long rem;

    int bits = 0;

    if (pow2_bits(page_bytes, &bits) != 0 || bits < 1 || length == 0) {
        return -1;
    }
    rem = length % page_bytes;
    n = length / page_bytes + (rem != 0);
    /* 真题只跨几页。一次印太多会把输出变成另一道题。 */
    if (n == 0 || n > 8) {
        return -1;
    }
    *count = (int)n;
    return 0;
}

int span_at(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long la,
            unsigned long long load_pa, unsigned long long table, int index, unsigned long long *page_no,
            unsigned long long *pte_addr, unsigned long long *frame, unsigned long long *pa) {
    unsigned long long first;
    unsigned long long delta;

    if (la % page_bytes != 0 || load_pa % page_bytes != 0) {
        return -1;
    }
    first = la / page_bytes;
    if (add_ull(first, (unsigned long long)index, page_no) != 0) {
        return -1;
    }
    if (addr_slot(pte_bytes, table, *page_no, pte_addr) != 0) {
        return -1;
    }
    if (mul_ull((unsigned long long)index, page_bytes, &delta) != 0 || add_ull(load_pa, delta, pa) != 0) {
        return -1;
    }
    *frame = *pa / page_bytes;
    return 0;
}

int addr_frag(unsigned long long page_bytes, unsigned long long bytes, unsigned long long *pages,
              unsigned long long *internal) {
    unsigned long long rem;
    int bits = 0;

    if (pow2_bits(page_bytes, &bits) != 0 || bits < 1 || bytes == 0) {
        return -1;
    }
    rem = bytes % page_bytes;
    *internal = (rem == 0) ? 0 : (page_bytes - rem);
    *pages = bytes / page_bytes + (rem != 0);
    return 0;
}
