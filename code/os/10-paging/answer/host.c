#include "geom.h"

#include <stdio.h>
#include <unistd.h>

/* 只把本机页大小代进王道的级数公式。虚拟地址用了多少位由 TCR 决定，这里不读寄存器。 */
static void report(int va_bits, unsigned long long page, unsigned long long pte) {
    Geom geom;

    if (geom_bits(va_bits, page, pte, &geom) != 0) {
        printf("assume va=%d pte=%llu result=bad-size\n", va_bits, pte);
        return;
    }
    printf("assume va=%d pte=%llu levels=%d top-bits=%d top-entries=%llu top-bytes=%llu fits=%s\n",
           va_bits, pte, geom.levels, geom.top_bits, geom.top_entries, geom.top_bytes,
           geom.fits ? "yes" : "no");
}

int main(void) {
    long page = getpagesize();

    printf("page=%ld\n", page);
    report(32, (unsigned long long)page, 4);
    report(64, (unsigned long long)page, 8);
    printf("note=formula-only\n");
    return 0;
}
