/*
 * 时间复杂度实验：把真题里的程序段原样抄下来，只在「基本语句」（最内层、执行最频繁的
 * 那一句）旁边加一个计数器。n 每扩大 10 倍，看计数器怎么变，就能和手推的阶对照：
 *   O(log n) 每次只多几次；O(√n) 约 ×3.16；O(n) ×10；O(n log n) 明显多于 ×10；O(n²) ×100
 */
#include "segments.h"

#include <string.h>

/* 2011 年第 1 题：x 每次翻倍，2^(t+1) >= n/2 时停，t ≈ log2(n) - 2，答案 O(log n) */
static long long seg_2011_1(int n) {
    long long cnt = 0;
    int x = 2;
    while (x < n / 2) {
        x = 2 * x;
        cnt++;
    }
    return cnt;
}

/* 2012 年第 1 题：fact(n) 每层调用自身一次、规模减 1，共调用 n 次（n <= 1 时 1 次），
 * T(n) = T(n-1) + 1 = O(n)。用 unsigned 是因为 1000! 会溢出，无符号溢出是有定义的回绕，
 * 这里只关心调用次数，不关心乘积。 */
static unsigned long long fact(int n, long long *cnt) {
    (*cnt)++;
    if (n <= 1) {
        return 1;
    }
    return (unsigned long long)n * fact(n - 1, cnt);
}

static long long seg_2012_1(int n) {
    long long cnt = 0;
    (void)fact(n, &cnt);
    return cnt;
}

/* 2014 年第 1 题：外层 k 倍增 floor(log2 n)+1 次，内层与 k 无关、固定 n 次，
 * 乘法规则得 O(n log n) */
static long long seg_2014_1(int n) {
    long long count = 0;
    for (int k = 1; k <= n; k *= 2) {
        for (int j = 1; j <= n; j++) {
            count++;
        }
    }
    return count;
}

/* 2017 年第 1 题：第 k 趟后 sum = k(k+1)/2，sum >= n 时停，k ≈ √(2n)，答案 O(√n)。
 * 循环趟数恰好就是函数返回的 i。 */
static long long seg_2017_1(int n) {
    int i = 0, sum = 0;
    while (sum < n) {
        sum += ++i;
    }
    return i;
}

/* 2019 年第 1 题：x 从 0 加到 floor(√n)，答案 O(√n) */
static long long seg_2019_1(int n) {
    long long cnt = 0;
    int x = 0;
    while (n >= (x + 1) * (x + 1)) {
        x = x + 1;
        cnt++;
    }
    return cnt;
}

/* 2022 年第 1 题：内层次数依赖外层 i，不能直接相乘；总次数 1+2+4+…+2^(r-1) = 2^r - 1 < 2n，
 * 答案 O(n)。常见错误是看到两层循环 + 倍增就选 O(n log n)。 */
static long long seg_2022_1(int n) {
    long long sum = 0;
    for (int i = 1; i < n; i *= 2) {
        for (int j = 0; j < i; j++) {
            sum++;
        }
    }
    return sum;
}

/* 练习：两层独立循环，n²，O(n²) */
static long long seg_square(int n) {
    long long cnt = 0;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            cnt++;
        }
    }
    return cnt;
}

/* 练习：内层上界依赖外层，1+2+…+n = n(n+1)/2，仍是 O(n²) */
static long long seg_triangle(int n) {
    long long cnt = 0;
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= i; j++) {
            cnt++;
        }
    }
    return cnt;
}

/* 练习：归并排序的骨架，T(n) = 2T(n/2) + n。递归树每层合计 n 次、共约 log2 n 层，
 * 所以是 O(n log n)；与 fact 对比：递归的复杂度 = 每层工作量 × 层数，而不只看递归深度。 */
static void halve(int n, long long *cnt) {
    if (n <= 1) {
        return;
    }
    halve(n / 2, cnt);
    halve(n - n / 2, cnt);
    for (int i = 0; i < n; i++) {
        (*cnt)++;
    }
}

static long long seg_halve(int n) {
    long long cnt = 0;
    halve(n, &cnt);
    return cnt;
}

typedef struct {
    const char *id;
    long long (*run)(int n);
} Segment;

static const Segment SEGMENTS[] = {
    {"2011-1", seg_2011_1}, {"2012-1", seg_2012_1}, {"2014-1", seg_2014_1},
    {"2017-1", seg_2017_1}, {"2019-1", seg_2019_1}, {"2022-1", seg_2022_1},
    {"square", seg_square}, {"triangle", seg_triangle}, {"halve", seg_halve},
};

bool segment_count(const char *id, int n, long long *count) {
    for (size_t i = 0; i < sizeof(SEGMENTS) / sizeof(SEGMENTS[0]); i++) {
        if (strcmp(SEGMENTS[i].id, id) == 0) {
            *count = SEGMENTS[i].run(n);
            return true;
        }
    }
    return false;
}
