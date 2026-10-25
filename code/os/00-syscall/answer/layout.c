/*
 * 打印本进程里几类地址，用来画内存映像。
 * 地址每次运行都可能变（ASLR），所以不进 tests，结果记在 EXPERIMENTS.md。
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int g_data = 1;
int g_bss;
static int s_bss;
static const char LITERAL[] = "literal";

static void marker(void) {}

typedef struct {
    const char *name;
    uintptr_t addr;
} Slot;

static void print_addr(const char *name, uintptr_t addr) {
    printf("%-10s 0x%llx\n", name, (unsigned long long)addr);
}

static void sort_slots(Slot *slots, int n) {
    for (int i = 1; i < n; i++) {
        Slot key = slots[i];
        int j = i;
        while (j > 0 && slots[j - 1].addr > key.addr) {
            slots[j] = slots[j - 1];
            j--;
        }
        slots[j] = key;
    }
}

static void print_order(const Slot *slots, int n) {
    printf("order");
    for (int i = 0; i < n; i++) {
        if (i == 0) {
            printf(" %s", slots[i].name);
            continue;
        }
        const char *sep = slots[i].addr > slots[i - 1].addr ? " < " : " = ";
        printf("%s%s", sep, slots[i].name);
    }
    printf("\n");
}

int main(void) {
    int local = 0;
    void *heap = malloc(16);
    if (heap == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    Slot slots[] = {
        {"text", (uintptr_t)(void (*)(void))marker},
        {"rodata", (uintptr_t)LITERAL},
        {"data", (uintptr_t)&g_data},
        {"bss", (uintptr_t)&g_bss},
        {"static-bss", (uintptr_t)&s_bss},
        {"heap", (uintptr_t)heap},
        {"stack", (uintptr_t)&local},
    };
    int n = (int)(sizeof slots / sizeof slots[0]);
    for (int i = 0; i < n; i++) {
        print_addr(slots[i].name, slots[i].addr);
    }
    sort_slots(slots, n);
    print_order(slots, n);
    free(heap);
    return 0;
}
