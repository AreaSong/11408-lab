#include "peterson.h"

#include <pthread.h>
#include <stdatomic.h>

/*
 * 关卡要求对照「加不加 atomic_thread_fence」。
 * C11 的屏障只排序原子对象，所以 flag 和 turn 用原子整数。
 * plain 用 relaxed，相当于普通读写，CPU 可以重排。
 * fence 在每次写和每次检查前后加 seq_cst 屏障，对齐王道默认的顺序一致性。
 */
static atomic_int g_flag[2];
static atomic_int g_turn;
static atomic_int g_inside;
static atomic_int g_bad;
static atomic_int g_counter;

static void reset_state(void) {
    atomic_store(&g_flag[0], 0);
    atomic_store(&g_flag[1], 0);
    atomic_store(&g_turn, 0);
    atomic_store(&g_inside, 0);
    atomic_store(&g_bad, 0);
    atomic_store(&g_counter, 0);
}

static void critical(void) {
    int now = atomic_fetch_add_explicit(&g_inside, 1, memory_order_relaxed) + 1;
    if (now != 1) {
        atomic_fetch_add_explicit(&g_bad, 1, memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&g_counter, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&g_inside, 1, memory_order_relaxed);
}

static void plain_enter(int self) {
    int other = 1 - self;
    atomic_store_explicit(&g_flag[self], 1, memory_order_relaxed);
    atomic_store_explicit(&g_turn, other, memory_order_relaxed);
    while (atomic_load_explicit(&g_flag[other], memory_order_relaxed) &&
           atomic_load_explicit(&g_turn, memory_order_relaxed) == other) {
    }
}

static void plain_leave(int self) {
    atomic_store_explicit(&g_flag[self], 0, memory_order_relaxed);
}

static void fence_enter(int self) {
    int other = 1 - self;
    atomic_store_explicit(&g_flag[self], 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    atomic_store_explicit(&g_turn, other, memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    for (;;) {
        int flag;
        int turn;
        atomic_thread_fence(memory_order_seq_cst);
        flag = atomic_load_explicit(&g_flag[other], memory_order_relaxed);
        turn = atomic_load_explicit(&g_turn, memory_order_relaxed);
        if (!(flag && turn == other)) {
            break;
        }
    }
}

static void fence_leave(int self) {
    atomic_thread_fence(memory_order_seq_cst);
    atomic_store_explicit(&g_flag[self], 0, memory_order_relaxed);
}

typedef struct {
    int self;
    int n;
    int fenced;
} PetArg;

static void *pet_worker(void *arg) {
    PetArg *a = arg;
    int i;
    for (i = 0; i < a->n; i++) {
        if (a->fenced) {
            fence_enter(a->self);
        } else {
            plain_enter(a->self);
        }
        critical();
        if (a->fenced) {
            fence_leave(a->self);
        } else {
            plain_leave(a->self);
        }
    }
    return NULL;
}

int peterson_run(int fenced, int n, int *got, int *bad) {
    PetArg a0;
    PetArg a1;
    pthread_t t0;
    pthread_t t1;
    a0.self = 0;
    a0.n = n;
    a0.fenced = fenced;
    a1.self = 1;
    a1.n = n;
    a1.fenced = fenced;
    reset_state();
    if (pthread_create(&t0, NULL, pet_worker, &a0) != 0) {
        return -1;
    }
    if (pthread_create(&t1, NULL, pet_worker, &a1) != 0) {
        pthread_join(t0, NULL);
        return -1;
    }
    pthread_join(t0, NULL);
    pthread_join(t1, NULL);
    *got = atomic_load(&g_counter);
    *bad = atomic_load(&g_bad);
    return 0;
}
