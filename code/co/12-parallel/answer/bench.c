#include "amdahl.h"
#include "cores.h"
#include "dep.h"
#include "frac.h"
#include "line.h"
#include "share.h"
#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <time.h>

enum {
    DEP_N = 1 << 22,
    DEP_REPS = 4,
    CORE_N = 1 << 21,
    CORE_ROUNDS = 32,
    CORE_PASSES = 4,
    SHARE_TIMES = 1 << 22,
    VEC_N = 1 << 20,
    VEC_ROUNDS = 32,
    VEC_REPS = 4
};

static long long wall_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static long long median3(long long a, long long b, long long c) {
    if (a > b) {
        long long t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        long long t = b;
        b = c;
        c = t;
    }
    if (a > b) {
        long long t = a;
        a = b;
        b = t;
    }
    return b;
}

static void print_span(long long ns) {
    printf("%lld ns（%lld.%03lld ms）", ns, ns / 1000000, (ns / 1000) % 1000);
}

static void print_ratio(long long num, long long den) {
    long long milli = (num * 1000 + den / 2) / den;
    printf("%lld.%03lld", milli / 1000, milli % 1000);
}

static int sys_int(const char *name, int *out) {
    int value = 0;
    size_t bytes = sizeof value;
    if (sysctlbyname(name, &value, &bytes, NULL, 0) != 0) {
        return -1;
    }
    *out = value;
    return 0;
}

/* hw.cachelinesize 是 8 字节。用 int 去接会因缓冲区太小而失败。 */
static int sys_u64(const char *name, unsigned long long *out) {
    unsigned long long value = 0;
    size_t bytes = sizeof value;
    if (sysctlbyname(name, &value, &bytes, NULL, 0) != 0 || bytes != sizeof value) {
        return -1;
    }
    *out = value;
    return 0;
}

static void show_machine(int *line_bytes) {
    char brand[128];
    size_t bytes = sizeof brand;
    int physical = -1;
    int perf = -1;
    int eff = -1;
    unsigned long long line = 0;
    if (sysctlbyname("machdep.cpu.brand_string", brand, &bytes, NULL, 0) != 0) {
        snprintf(brand, sizeof brand, "未知");
    }
    sys_int("hw.physicalcpu", &physical);
    sys_int("hw.perflevel0.physicalcpu", &perf);
    sys_int("hw.perflevel1.physicalcpu", &eff);
    printf("【本机】\n");
    printf("型号 = %s\n", brand);
    printf("物理 CPU = %d 个，性能核 = %d 个，能效核 = %d 个\n", physical, perf, eff);
    if (sys_u64("hw.cachelinesize", &line) != 0 || line == 0 || line > 4096) {
        line = 128;
        printf("缓存行 = 128 B（sysctl 读不到，按 Apple M 系列）\n");
    } else {
        printf("缓存行 = %llu B\n", line);
    }
    printf("纳秒数随当次调度变化，不进 tests/\n");
    *line_bytes = (int)line;
}

static long long time_calls(double (*fn)(const double *, int), const double *a, int reps, double *sink) {
    long long t0 = wall_ns();
    double sum = 0.0;
    int i;
    for (i = 0; i < reps; i++) {
        sum += fn(a, DEP_N);
    }
    *sink = sum;
    return wall_ns() - t0;
}

static long long time_array(const double *a, double *b, int reps, double *sink) {
    long long t0 = wall_ns();
    double sum = 0.0;
    int i;
    for (i = 0; i < reps; i++) {
        sum += dep_array(a, b, DEP_N);
    }
    *sink = sum;
    return wall_ns() - t0;
}

static void show_one_dep(const char *title, long long ns, double sink) {
    printf("%s = ", title);
    print_span(ns);
    printf("，校验 = %.9g\n", sink);
}

static void show_dep(const double *a, double *b) {
    double chain_sum = 0;
    double ilp_sum = 0;
    double array_sum = 0;
    long long chain = median3(time_calls(dep_chain, a, DEP_REPS, &chain_sum),
                               time_calls(dep_chain, a, DEP_REPS, &chain_sum),
                               time_calls(dep_chain, a, DEP_REPS, &chain_sum));
    long long ilp = median3(time_calls(dep_ilp, a, DEP_REPS, &ilp_sum),
                            time_calls(dep_ilp, a, DEP_REPS, &ilp_sum),
                            time_calls(dep_ilp, a, DEP_REPS, &ilp_sum));
    long long stored = median3(time_array(a, b, DEP_REPS, &array_sum),
                               time_array(a, b, DEP_REPS, &array_sum),
                               time_array(a, b, DEP_REPS, &array_sum));
    printf("【① 数据相关】\n");
    printf("元素 = %d 个 double，每种重复 %d 遍，取 3 次中位数。数组里全是 1\n", DEP_N, DEP_REPS);
    show_one_dep("相关链（一个累加器）", chain, chain_sum);
    show_one_dep("四个独立累加器", ilp, ilp_sum);
    show_one_dep("独立数组写回", stored, array_sum);
    printf("相关链 / 四累加器 = ");
    print_ratio(chain, ilp);
    printf("\n");
    printf("四条乘法链互不依赖，超标量可以同时做。相关链必须等上一次乘法的结果\n");
    printf("这仍是 SISD：一个核、一条指令流。数组写回还要占存储带宽，快慢不单由相关决定\n");
}

static long long median_cores(const double *a, double *b, int nthread, double *check) {
    long long s0 = cores_run(a, b, CORE_N, nthread, CORE_ROUNDS, CORE_PASSES, check);
    long long s1 = cores_run(a, b, CORE_N, nthread, CORE_ROUNDS, CORE_PASSES, check);
    long long s2 = cores_run(a, b, CORE_N, nthread, CORE_ROUNDS, CORE_PASSES, check);
    if (s0 < 0 || s1 < 0 || s2 < 0) {
        fprintf(stderr, "线程启动失败\n");
        exit(1);
    }
    return median3(s0, s1, s2);
}

static void show_cores(const double *a, double *b) {
    static const int threads[] = {1, 2, 4, 8};
    long long base = 0;
    double first = 0.0;
    int i;
    Amdahl full = amdahl_of(100, 8);
    Amdahl nine = amdahl_of(90, 8);
    printf("【② 多核加速比】\n");
    printf("元素 = %d 个，每个元素 %d 次乘加，再重复 %d 遍\n", CORE_N, CORE_ROUNDS, CORE_PASSES);
    printf("加速比 = 1 线程墙钟 / 本行墙钟。线程设成用户交互优先级，尽量落在性能核上\n");
    for (i = 0; i < 4; i++) {
        double check = 0.0;
        long long ns = median_cores(a, b, threads[i], &check);
        if (i == 0) {
            base = ns;
            first = check;
        }
        printf("%d 线程 = ", threads[i]);
        print_span(ns);
        printf("，加速比 = ");
        print_ratio(base, ns);
        printf("，b[0] = %.9g\n", check);
        if (check != first) {
            printf("b[0] 与 1 线程不一致\n");
        }
    }
    printf("Amdahl：p = 100%% 时 8 核加速比 = ");
    frac_write(stdout, full.speed_num, full.speed_den);
    printf("；p = 90%% 时 8 核加速比 = ");
    frac_write(stdout, nine.speed_num, nine.speed_den);
    printf("，极限 = ");
    frac_write(stdout, nine.limit_num, nine.limit_den);
    printf("\n这一步是 MIMD。p = 100%% 时加速比的上界是核数；比上界小是开销，偶尔大一点是计时抖动\n");
}

static const char *kind_name(int kind) {
    if (kind == LINE_FALSE) {
        return "伪共享";
    }
    if (kind == LINE_TRUE) {
        return "真共享";
    }
    return "不共享";
}

static long long show_share_one(const char *title, int apart, int line_bytes) {
    ShareResult a;
    ShareResult b;
    ShareResult c;
    unsigned long long line = (unsigned long long)line_bytes;
    long long ns;
    int kind;
    if (share_run(apart, SHARE_TIMES, &a) != 0 || share_run(apart, SHARE_TIMES, &b) != 0 ||
        share_run(apart, SHARE_TIMES, &c) != 0) {
        fprintf(stderr, "伪共享实验启动失败\n");
        exit(1);
    }
    ns = median3(a.ns, b.ns, c.ns);
    kind = line_kind(line, a.addr_a, a.addr_b);
    printf("%s：地址差 = %d B，行号差 = %llu，判断 = %s，", title, apart, a.addr_b / line - a.addr_a / line,
           kind_name(kind));
    print_span(ns);
    printf("，计数 = %d 和 %d\n", c.value_a, c.value_b);
    return ns;
}

static void show_share(int line_bytes) {
    long long near;
    long long far;
    printf("【③ 伪共享】\n");
    printf("两个线程各把自己的 int 加 %d 次。缓存行按本机 %d B 判断\n", SHARE_TIMES, line_bytes);
    printf("用的是宽松原子加，避免数据竞争；独占一行的权限仍然要抢\n");
    near = show_share_one("相邻 int", (int)sizeof(int), line_bytes);
    far = show_share_one("间隔 128 B", 128, line_bytes);
    printf("相邻 / 间隔 = ");
    print_ratio(near, far);
    printf("\n相邻时两个 int 落在同一行，一个核写入会让另一个核的整行失效\n");
    printf("间隔 128 B 后行号不同。两个线程始终没有写同一个变量\n");
}

static long long time_scalar(const float *a, float *c) {
    long long t0 = wall_ns();
    int i;
    for (i = 0; i < VEC_REPS; i++) {
        vec_scalar(a, c, VEC_N, VEC_ROUNDS);
    }
    return wall_ns() - t0;
}

static long long time_neon(const float *a, float *c) {
    long long t0 = wall_ns();
    int i;
    for (i = 0; i < VEC_REPS; i++) {
        vec_neon(a, c, VEC_N, VEC_ROUNDS);
    }
    return wall_ns() - t0;
}

static float spot(const float *c) {
    return c[0] + c[VEC_N / 2] + c[VEC_N - 1];
}

static void show_vec(const float *a, float *scalar_out, float *neon_out) {
    long long scalar_ns = median3(time_scalar(a, scalar_out), time_scalar(a, scalar_out),
                                  time_scalar(a, scalar_out));
    long long neon_ns = median3(time_neon(a, neon_out), time_neon(a, neon_out), time_neon(a, neon_out));
    printf("【④ SIMD】\n");
    printf("元素 = %d 个 float，每个元素 %d 次乘加，重复 %d 遍\n", VEC_N, VEC_ROUNDS, VEC_REPS);
    printf("标量循环已关掉自动向量化。NEON 是 128 bit，一条指令 4 个 float\n");
    printf("标量 = ");
    print_span(scalar_ns);
    printf("，校验 = %.8g\n", spot(scalar_out));
    printf("NEON = ");
    print_span(neon_ns);
    printf("，校验 = %.8g\n", spot(neon_out));
    printf("标量 / NEON = ");
    print_ratio(scalar_ns, neon_ns);
    printf("\n这一步是 SIMD，和步骤 ① 的四累加器不是一回事\n");
}

static void fill_double(double *a, int n) {
    int i;
    for (i = 0; i < n; i++) {
        a[i] = 1.0;
    }
}

static void fill_float(float *a, int n) {
    int i;
    for (i = 0; i < n; i++) {
        a[i] = 1.0f;
    }
}

int main(void) {
    int line_bytes = 128;
    double *a = malloc((size_t)DEP_N * sizeof(double));
    double *b = malloc((size_t)DEP_N * sizeof(double));
    float *fa = malloc((size_t)VEC_N * sizeof(float));
    float *fs = malloc((size_t)VEC_N * sizeof(float));
    float *fn = malloc((size_t)VEC_N * sizeof(float));
    if (a == NULL || b == NULL || fa == NULL || fs == NULL || fn == NULL) {
        fprintf(stderr, "内存不足\n");
        return 1;
    }
    fill_double(a, DEP_N);
    fill_float(fa, VEC_N);
    show_machine(&line_bytes);
    show_dep(a, b);
    show_cores(a, b);
    show_share(line_bytes);
    show_vec(fa, fs, fn);
    free(a);
    free(b);
    free(fa);
    free(fs);
    free(fn);
    return 0;
}
