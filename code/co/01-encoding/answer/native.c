/*
 * 小步骤 ④⑤：用指针看本机字节序，用 sizeof / offsetof 核对结构体。
 * 输出依赖本机 ABI，不进 tests/，记录在 EXPERIMENTS.md。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct rec2012 {
    int a;
    char b;
    short c;
};

struct rec2020 {
    short x1;
    int x2;
};

struct rec2025 {
    int id;
    char name[10];
    int salary;
};

struct host_long {
    char c;
    long v;
};

struct host_double {
    char c;
    double v;
};

static void dump_bytes(const char *title, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    size_t i;
    printf("%s\n", title);
    for (i = 0; i < n; i++) {
        printf("  偏移 %zu：%02X\n", i, b[i]);
    }
}

static void expect_size(const char *name, size_t got, size_t want) {
    printf("%s = %zu，卷面 %zu，%s\n", name, got, want, got == want ? "一致" : "不一致");
}

int main(void) {
    uint32_t word = 0x12345678u;
    const unsigned char *p = (const unsigned char *)&word;
    dump_bytes("uint32_t 机器数 12345678H", &word, sizeof word);
    if (p[0] == 0x78) {
        printf("首字节是 78H，低字节在低地址，这台机器是小端\n");
    } else if (p[0] == 0x12) {
        printf("首字节是 12H，高字节在低地址，这台机器是大端\n");
    } else {
        printf("首字节是 %02XH\n", p[0]);
    }
    expect_size("2012 sizeof", sizeof(struct rec2012), 8);
    expect_size("2012 offsetof c", offsetof(struct rec2012, c), 6);
    expect_size("2020 sizeof", sizeof(struct rec2020), 8);
    expect_size("2020 offsetof x2", offsetof(struct rec2020, x2), 4);
    expect_size("2025 sizeof", sizeof(struct rec2025), 20);
    expect_size("2025 offsetof salary", offsetof(struct rec2025, salary), 16);
    printf("含 long：sizeof %zu，long 的偏移 %zu；sizeof(long) = %zu，sizeof(void *) = %zu\n",
           sizeof(struct host_long), offsetof(struct host_long, v), sizeof(long), sizeof(void *));
    printf("卷面 32 位：long 为 4 字节，char 后填充 3，偏移 4，结构体大小 8\n");
    printf("含 double：sizeof %zu，double 的偏移 %zu\n", sizeof(struct host_double), offsetof(struct host_double, v));
    printf("王道：double 按 8 对齐，char 后填充 7，偏移 8，结构体大小 16\n");
    return 0;
}
