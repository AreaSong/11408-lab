/*
 * 按步长和数据量做指针追逐，看延迟随工作集变大跳出的台阶。
 * 顺序扫描会被预取器藏住，所以结点按步长排开后随机串成环。
 * 计时依赖本机和调度到哪类核，不进 tests/。
 */
#include <pthread/qos.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

static uint64_t cache_size(const char *name) {
    uint64_t value = 0;
    size_t len = sizeof value;
    if (sysctlbyname(name, &value, &len, NULL, 0) != 0) {
        return 0;
    }
    return value;
}

static uint32_t mix(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void shuffle(size_t *order, size_t n) {
    uint32_t state = 1;
    size_t i;
    for (i = 0; i < n; i++) {
        order[i] = i;
    }
    for (i = n; i > 1; i--) {
        size_t j = mix(&state) % i;
        size_t tmp = order[i - 1];
        order[i - 1] = order[j];
        order[j] = tmp;
    }
}

static void *chase(void *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        p = *(void *volatile *)p;
    }
    return p;
}

static int build_ring(char *buf, size_t n, size_t stride) {
    size_t *order = malloc(n * sizeof *order);
    size_t i;
    if (order == NULL) {
        return 1;
    }
    shuffle(order, n);
    for (i = 0; i < n; i++) {
        size_t next = order[(i + 1) % n];
        *(void **)(buf + order[i] * stride) = buf + next * stride;
    }
    free(order);
    return 0;
}

static double nanoseconds(void *start, size_t reps, void **sink) {
    struct timespec begin;
    struct timespec end;
    void *p;
    clock_gettime(CLOCK_MONOTONIC, &begin);
    p = chase(start, reps);
    clock_gettime(CLOCK_MONOTONIC, &end);
    *sink = p;
    return (double)(end.tv_sec - begin.tv_sec) * 1e9 + (double)(end.tv_nsec - begin.tv_nsec);
}

static void pin_class(char which) {
    qos_class_t qos = which == 'e' ? QOS_CLASS_BACKGROUND : QOS_CLASS_USER_INTERACTIVE;
    if (pthread_set_qos_class_self_np(qos, 0) != 0) {
        fprintf(stderr, "没能钉到指定的核等级，结果可能落在另一类核上\n");
    }
}

static const char *where(uint64_t bytes, uint64_t l1, uint64_t l2) {
    if (l1 != 0 && bytes <= l1) {
        return "不超过这一级核的 L1 数据 Cache";
    }
    if (l2 != 0 && bytes <= l2) {
        return "超过 L1，不超过这一级核的 L2";
    }
    if (l2 != 0) {
        return "超过这一级核的 L2，主要落到主存";
    }
    return "没有读到可用于对照的 Cache 容量";
}

static void print_caches(void) {
    printf("hw.l1dcachesize = %llu B\n", (unsigned long long)cache_size("hw.l1dcachesize"));
    printf("hw.l2cachesize = %llu B\n", (unsigned long long)cache_size("hw.l2cachesize"));
    printf("hw.cachelinesize = %llu B\n", (unsigned long long)cache_size("hw.cachelinesize"));
    printf("性能核 L1 数据 = %llu B，L2 = %llu B\n",
           (unsigned long long)cache_size("hw.perflevel0.l1dcachesize"),
           (unsigned long long)cache_size("hw.perflevel0.l2cachesize"));
    printf("能效核 L1 数据 = %llu B，L2 = %llu B\n",
           (unsigned long long)cache_size("hw.perflevel1.l1dcachesize"),
           (unsigned long long)cache_size("hw.perflevel1.l2cachesize"));
}

static int parse_args(int argc, char **argv, size_t *bytes, size_t *stride, char *which) {
    unsigned long long bytes_arg = 0;
    unsigned long long stride_arg = 0;
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "用法：%s <字节数> <步长> [p|e]\n", argv[0]);
        return 1;
    }
    if (sscanf(argv[1], "%llu", &bytes_arg) != 1 || sscanf(argv[2], "%llu", &stride_arg) != 1) {
        fprintf(stderr, "字节数和步长要是正整数\n");
        return 1;
    }
    *which = 'p';
    if (argc == 4 && (argv[3][0] == 'e' || argv[3][0] == 'p') && argv[3][1] == '\0') {
        *which = argv[3][0];
    } else if (argc == 4) {
        fprintf(stderr, "核等级只能是 p（性能核）或 e（能效核）\n");
        return 1;
    }
    *bytes = (size_t)bytes_arg;
    *stride = (size_t)stride_arg;
    if (*stride < sizeof(void *) || *bytes % *stride != 0 || *bytes % 128 != 0 || *bytes / *stride < 2) {
        fprintf(stderr, "步长至少要放下一个指针，字节数要同时是步长和 128 的倍数，结点至少 2 个\n");
        return 1;
    }
    return 0;
}

static void report(size_t bytes, size_t stride, char which, size_t reps, double elapsed) {
    uint64_t l1 = cache_size(which == 'e' ? "hw.perflevel1.l1dcachesize" : "hw.perflevel0.l1dcachesize");
    uint64_t l2 = cache_size(which == 'e' ? "hw.perflevel1.l2cachesize" : "hw.perflevel0.l2cachesize");
    if (l1 == 0) {
        l1 = cache_size("hw.l1dcachesize");
        l2 = cache_size("hw.l2cachesize");
    }
    print_caches();
    printf("核等级 = %s\n", which == 'e' ? "能效核" : "性能核");
    printf("数据量 = %llu B，步长 = %llu B，结点 = %llu\n", (unsigned long long)bytes,
           (unsigned long long)stride, (unsigned long long)(bytes / stride));
    printf("计时访问 = %llu 次，总时间 = %.0f ns\n", (unsigned long long)reps, elapsed);
    printf("平均每次 = %.2f ns\n", elapsed / (double)reps);
    printf("对照：%s\n", where(bytes, l1, l2));
}

int main(int argc, char **argv) {
    size_t bytes = 0;
    size_t stride = 0;
    size_t nodes;
    size_t reps;
    char which = 'p';
    char *buf;
    void *sink = NULL;
    if (parse_args(argc, argv, &bytes, &stride, &which) != 0) {
        return 1;
    }
    pin_class(which);
    buf = aligned_alloc(128, bytes);
    if (buf == NULL || build_ring(memset(buf, 0, bytes), nodes = bytes / stride, stride) != 0) {
        fprintf(stderr, "准备访存缓冲区失败\n");
        free(buf);
        return 1;
    }
    reps = nodes * 16 < 2000000 ? 2000000 : nodes * 16;
    sink = chase(buf, nodes);
    report(bytes, stride, which, reps, nanoseconds(sink, reps, &sink));
    free(buf);
    return sink == NULL ? 1 : 0;
}
