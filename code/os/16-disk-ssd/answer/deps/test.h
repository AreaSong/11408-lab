/*
 * test.h · 所有关卡共用的极简测试宏（只有头文件，直接 #include 即可）
 *
 * 用法：
 *   #include "test.h"
 *   int main(void) {
 *       CHECK(1 + 1 == 2);              // 条件为假时打印文件名、行号和表达式
 *       CHECK_EQ(max, 9);               // 整数相等，失败时额外打印两边的实际值
 *       CHECK_STR(name, "seqlist");     // 字符串相等（strcmp），允许 NULL
 *       return TEST_SUMMARY();          // 全部通过返回 0，否则返回 1，CTest 据此判定
 *   }
 *
 * 失败信息写到 stderr、汇总写到 stdout：这样失败的检查不会中断后面的检查，
 * 一次运行就能看到所有出错的地方。
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

typedef struct {
    int total;
    int failed;
} TestStats;

/* 计数器放在 static inline 函数的局部静态变量里：
 * 头文件里直接定义 static 全局变量，没用到它的文件会报 unused 警告。 */
static inline TestStats *test_stats_(void) {
    static TestStats stats = {0, 0};
    return &stats;
}

static inline void test_report_(int ok, const char *expr, const char *file, int line) {
    TestStats *s = test_stats_();
    s->total++;
    if (!ok) {
        s->failed++;
        fprintf(stderr, "%s:%d: CHECK 失败：%s\n", file, line, expr);
    }
}

static inline void test_check_eq_(long long actual, long long expected,
                                  const char *expr, const char *file, int line) {
    test_report_(actual == expected, expr, file, line);
    if (actual != expected) {
        fprintf(stderr, "    实际值 %lld，期望值 %lld\n", actual, expected);
    }
}

static inline void test_check_str_(const char *actual, const char *expected,
                                   const char *expr, const char *file, int line) {
    int ok = (actual == NULL || expected == NULL) ? actual == expected
                                                  : strcmp(actual, expected) == 0;
    test_report_(ok, expr, file, line);
    if (!ok) {
        fprintf(stderr, "    实际值 \"%s\"，期望值 \"%s\"\n",
                actual ? actual : "(null)", expected ? expected : "(null)");
    }
}

static inline int test_summary_(void) {
    TestStats *s = test_stats_();
    if (s->failed == 0) {
        printf("[PASS] %d 项检查全部通过\n", s->total);
        return 0;
    }
    printf("[FAIL] %d / %d 项检查失败\n", s->failed, s->total);
    return 1;
}

#define CHECK(cond) test_report_(!!(cond), #cond, __FILE__, __LINE__)

/* 两边都转成 long long 比较，int、size_t、char 都能直接用 */
#define CHECK_EQ(actual, expected)                                              \
    test_check_eq_((long long)(actual), (long long)(expected),                 \
                   #actual " == " #expected, __FILE__, __LINE__)

#define CHECK_STR(actual, expected)                                             \
    test_check_str_((actual), (expected), #actual " == " #expected, __FILE__, __LINE__)

#define TEST_SUMMARY() test_summary_()

#endif
