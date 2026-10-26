#include "line.h"

int line_kind(unsigned long long size, unsigned long long a, unsigned long long b) {
    if (a == b) {
        return LINE_TRUE;
    }
    if (a / size == b / size) {
        return LINE_FALSE;
    }
    return LINE_NONE;
}

static void show_addr(FILE *fp, char name, unsigned long long size, unsigned long long addr) {
    fprintf(fp, "地址 %c = %llu，行号 = %llu，行内偏移 = %llu B\n", name, addr, addr / size,
            addr % size);
}

static void show_kind(FILE *fp, int kind) {
    if (kind == LINE_TRUE) {
        fputs("两个地址相同。两个核写的是同一个变量，这是真共享\n", fp);
        fputs("结果：真共享\n", fp);
        return;
    }
    if (kind == LINE_FALSE) {
        fputs("地址不同，行号相同。一致性维护的单位是整行，另一个核上相邻的数据也会被赶出去\n", fp);
        fputs("程序并没有共享同一个变量，硬件却按共享了一整行来处理，这就是伪共享\n", fp);
        fputs("结果：伪共享\n", fp);
        return;
    }
    fputs("行号不同。各写各的缓存行，不会因为对方的写而失效\n", fp);
    fputs("结果：不共享\n", fp);
}

void line_show(FILE *fp, unsigned long long size, unsigned long long a, unsigned long long b) {
    fputs("【缓存行 · 两个地址会不会互相踢】\n", fp);
    fprintf(fp, "缓存行 = %llu B\n", size);
    show_addr(fp, 'A', size, a);
    show_addr(fp, 'B', size, b);
    show_kind(fp, line_kind(size, a, b));
}
