#ifndef RELOC_H
#define RELOC_H

/*
 * 连续分配的两种保护。界地址（限长）先和逻辑地址比，合法才加重定位寄存器。
 * 上下限寄存器直接圈物理地址，两端都含在内。
 * 返回 0 表示寄存器合法；*ok 为 1 表示这次访问没越界。
 */
int reloc_logical(long long base, long long limit, long long logical, int *ok,
                  long long *physical);
int reloc_bounds(long long low, long long high, long long addr, int *ok);

#endif
