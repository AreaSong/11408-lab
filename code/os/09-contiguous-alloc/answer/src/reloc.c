#include "reloc.h"

int reloc_logical(long long base, long long limit, long long logical, int *ok,
                  long long *physical) {
    if (base < 0 || limit < 0 || logical < 0 || ok == 0) {
        return -1;
    }
    /* 合法逻辑地址是 [0, limit)。等于限长已经越出这一块。 */
    if (logical >= limit) {
        *ok = 0;
        return 0;
    }
    *ok = 1;
    if (physical != 0) {
        *physical = base + logical;
    }
    return 0;
}

int reloc_bounds(long long low, long long high, long long addr, int *ok) {
    if (low < 0 || high < 0 || low > high || addr < 0 || ok == 0) {
        return -1;
    }
    /* 上下限存的是物理地址，且上限是分区最后一个单元，所以是闭区间。 */
    *ok = addr >= low && addr <= high;
    return 0;
}
