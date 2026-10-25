#include "vmm_type.h"

#include <string.h>

/*
 * 第一类直接占着硬件，性能更好，虚拟机搬家麻烦。
 * 第二类是宿主操作系统上的程序，虚拟磁盘往往就是宿主里的一个大文件，所以好迁移，
 * 但每次都要宿主转一手，性能差一档。
 * 两类的特权级都高于客户操作系统。第二类看起来像普通进程，截获客户机特权指令时
 * 仍然要进更高的特权级，否则 2025-24 的 C 就不会是错句。
 */
static const VmmFact VMM_ROWS[] = {
    {"type1", "hardware", "no", "high", "hard", "highest"},
    {"type2", "host-os", "yes", "lower", "easy", "above-guest"},
};

bool vmm_lookup(const char *name, const VmmFact **fact) {
    size_t n = sizeof VMM_ROWS / sizeof VMM_ROWS[0];
    size_t i;
    if (name == NULL || fact == NULL) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(VMM_ROWS[i].name, name) == 0) {
            *fact = &VMM_ROWS[i];
            return true;
        }
    }
    return false;
}
