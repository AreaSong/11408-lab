#ifndef PARALLEL_BARRIER_H
#define PARALLEL_BARRIER_H

#include <pthread.h>

/* macOS 没有 pthread_barrier_t。凑齐 need 个等待者再一起放行。 */
typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int need;
    int arrived;
    int phase;
} Barrier;

static inline void barrier_init(Barrier *b, int need) {
    pthread_mutex_init(&b->mu, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->need = need;
    b->arrived = 0;
    b->phase = 0;
}

static inline void barrier_wait(Barrier *b) {
    int phase;
    pthread_mutex_lock(&b->mu);
    phase = b->phase;
    b->arrived++;
    if (b->arrived == b->need) {
        b->arrived = 0;
        b->phase++;
        pthread_cond_broadcast(&b->cv);
    } else {
        while (b->phase == phase) {
            pthread_cond_wait(&b->cv, &b->mu);
        }
    }
    pthread_mutex_unlock(&b->mu);
}

static inline void barrier_destroy(Barrier *b) {
    pthread_mutex_destroy(&b->mu);
    pthread_cond_destroy(&b->cv);
}

#endif
