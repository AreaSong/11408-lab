#include "trial.h"

#include "sem.h"
#include "spin.h"

#include <pthread.h>
#include <stdatomic.h>

/* 故意不加锁的共享计数。volatile 只拦住编译器删循环，挡不住两个核同时读改写。 */
static volatile int g_bare;

typedef struct {
    int n;
} BareArg;

static void *bare_worker(void *arg) {
    BareArg *a = arg;
    int i;
    for (i = 0; i < a->n; i++) {
        g_bare++;
    }
    return NULL;
}

int trial_bare(int n, int *got) {
    BareArg a0;
    BareArg a1;
    pthread_t t0;
    pthread_t t1;
    a0.n = n;
    a1.n = n;
    g_bare = 0;
    if (pthread_create(&t0, NULL, bare_worker, &a0) != 0) {
        return -1;
    }
    if (pthread_create(&t1, NULL, bare_worker, &a1) != 0) {
        pthread_join(t0, NULL);
        return -1;
    }
    pthread_join(t0, NULL);
    pthread_join(t1, NULL);
    *got = g_bare;
    return 0;
}

typedef struct {
    int n;
    int waste;
    int kind;
    Tsl *tsl;
    SwapLock *swap;
    pthread_mutex_t *mu;
    Sem *sem;
    atomic_int *counter;
} LockArg;

static void burn(int loops) {
    volatile int x = 0;
    int i;
    for (i = 0; i < loops; i++) {
        x += 1;
    }
}

static void enter_lock(LockArg *a) {
    if (a->kind == TRIAL_TSL) {
        tsl_lock(a->tsl);
    } else if (a->kind == TRIAL_SWAP) {
        swap_lock(a->swap);
    } else if (a->kind == TRIAL_MUTEX) {
        pthread_mutex_lock(a->mu);
    } else {
        sem_P(a->sem);
    }
}

static void leave_lock(LockArg *a) {
    if (a->kind == TRIAL_TSL) {
        tsl_unlock(a->tsl);
    } else if (a->kind == TRIAL_SWAP) {
        swap_unlock(a->swap);
    } else if (a->kind == TRIAL_MUTEX) {
        pthread_mutex_unlock(a->mu);
    } else {
        sem_V(a->sem);
    }
}

static void *lock_worker(void *arg) {
    LockArg *a = arg;
    int i;
    for (i = 0; i < a->n; i++) {
        enter_lock(a);
        atomic_fetch_add_explicit(a->counter, 1, memory_order_relaxed);
        if (a->waste > 0) {
            burn(a->waste);
        }
        leave_lock(a);
    }
    return NULL;
}

int trial_lock(int kind, int n, int waste, int *got) {
    Tsl tsl = {ATOMIC_FLAG_INIT};
    SwapLock swap;
    pthread_mutex_t mu;
    Sem sem;
    atomic_int counter;
    LockArg arg;
    pthread_t t0;
    pthread_t t1;
    int rc = 0;
    int use_sem = kind == TRIAL_SEM;
    int use_mu = kind == TRIAL_MUTEX;
    swap_init(&swap);
    atomic_init(&counter, 0);
    if (use_mu && pthread_mutex_init(&mu, NULL) != 0) {
        return -1;
    }
    if (use_sem && sem_init(&sem, 1) != 0) {
        if (use_mu) {
            pthread_mutex_destroy(&mu);
        }
        return -1;
    }
    arg.n = n;
    arg.waste = waste;
    arg.kind = kind;
    arg.tsl = &tsl;
    arg.swap = &swap;
    arg.mu = &mu;
    arg.sem = &sem;
    arg.counter = &counter;
    if (pthread_create(&t0, NULL, lock_worker, &arg) != 0) {
        rc = -1;
    } else if (pthread_create(&t1, NULL, lock_worker, &arg) != 0) {
        pthread_join(t0, NULL);
        rc = -1;
    } else {
        pthread_join(t0, NULL);
        pthread_join(t1, NULL);
        *got = atomic_load(&counter);
        rc = 0;
    }
    if (use_mu) {
        pthread_mutex_destroy(&mu);
    }
    if (use_sem) {
        sem_destroy(&sem);
    }
    return rc;
}
