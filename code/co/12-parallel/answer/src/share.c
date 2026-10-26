#include "share.h"

#include "barrier.h"

#include <pthread.h>
#include <pthread/qos.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

_Static_assert(sizeof(atomic_int) == 4, "atomic_int 要是 4 字节，间隔才能按缓存行对齐");

enum { SHARE_CELLS = 64 };

_Alignas(128) static atomic_int cells[SHARE_CELLS];

typedef struct {
    atomic_int *slot;
    int times;
    Barrier *gate;
} ShareJob;

static void *share_worker(void *arg) {
    ShareJob *job = arg;
    int i;
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    barrier_wait(job->gate);
    for (i = 0; i < job->times; i++) {
        atomic_fetch_add_explicit(job->slot, 1, memory_order_relaxed);
    }
    barrier_wait(job->gate);
    return NULL;
}

static long long wall_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

int share_run(int apart_bytes, int times, ShareResult *out) {
    int index = apart_bytes / (int)sizeof(atomic_int);
    pthread_t th[2];
    ShareJob jobs[2];
    Barrier gate;
    long long t0;
    long long t1;
    int t;
    if (apart_bytes < (int)sizeof(atomic_int) || apart_bytes % (int)sizeof(atomic_int) != 0 ||
        index >= SHARE_CELLS || times < 1) {
        return -1;
    }
    atomic_store_explicit(&cells[0], 0, memory_order_relaxed);
    atomic_store_explicit(&cells[index], 0, memory_order_relaxed);
    barrier_init(&gate, 3);
    jobs[0].slot = &cells[0];
    jobs[1].slot = &cells[index];
    for (t = 0; t < 2; t++) {
        jobs[t].times = times;
        jobs[t].gate = &gate;
        if (pthread_create(&th[t], NULL, share_worker, &jobs[t]) != 0) {
            barrier_destroy(&gate);
            return -1;
        }
    }
    barrier_wait(&gate);
    t0 = wall_ns();
    barrier_wait(&gate);
    t1 = wall_ns();
    pthread_join(th[0], NULL);
    pthread_join(th[1], NULL);
    barrier_destroy(&gate);
    out->ns = t1 - t0;
    out->addr_a = (unsigned long long)(uintptr_t)&cells[0];
    out->addr_b = (unsigned long long)(uintptr_t)&cells[index];
    out->value_a = atomic_load_explicit(&cells[0], memory_order_relaxed);
    out->value_b = atomic_load_explicit(&cells[index], memory_order_relaxed);
    return 0;
}
