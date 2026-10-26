#include "cores.h"

#include "barrier.h"

#include <pthread.h>
#include <pthread/qos.h>
#include <time.h>

typedef struct {
    const double *a;
    double *b;
    int begin;
    int end;
    int rounds;
    int passes;
    Barrier *gate;
} CoreJob;

static void work_range(CoreJob *job) {
    int pass;
    int i;
    for (pass = 0; pass < job->passes; pass++) {
        /* 每个元素自己的乘法链不跨元素。关掉向量化，避免和步骤 ④ 混在一起。 */
#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)
        for (i = job->begin; i < job->end; i++) {
            double x = job->a[i];
            int k;
            for (k = 0; k < job->rounds; k++) {
                x = x * 1.0000001192092896 + 0.9999998807907104;
            }
            job->b[i] = x;
        }
    }
}

static void *core_worker(void *arg) {
    CoreJob *job = arg;
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    barrier_wait(job->gate);
    work_range(job);
    barrier_wait(job->gate);
    return NULL;
}

static long long wall_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

long long cores_run(const double *a, double *b, int n, int nthread, int rounds, int passes,
                    double *checksum) {
    pthread_t th[8];
    CoreJob jobs[8];
    Barrier gate;
    int span;
    int t;
    long long t0;
    long long t1;
    if (nthread < 1 || nthread > 8 || rounds < 1 || passes < 1 || n % nthread != 0) {
        return -1;
    }
    span = n / nthread;
    barrier_init(&gate, nthread + 1);
    for (t = 0; t < nthread; t++) {
        jobs[t].a = a;
        jobs[t].b = b;
        jobs[t].begin = t * span;
        jobs[t].end = jobs[t].begin + span;
        jobs[t].rounds = rounds;
        jobs[t].passes = passes;
        jobs[t].gate = &gate;
        if (pthread_create(&th[t], NULL, core_worker, &jobs[t]) != 0) {
            barrier_destroy(&gate);
            return -1;
        }
    }
    barrier_wait(&gate);
    t0 = wall_ns();
    barrier_wait(&gate);
    t1 = wall_ns();
    for (t = 0; t < nthread; t++) {
        pthread_join(th[t], NULL);
    }
    barrier_destroy(&gate);
    *checksum = b[0];
    return t1 - t0;
}
