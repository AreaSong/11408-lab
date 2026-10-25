#include "sem.h"

int sem_init(Sem *s, int value) {
    s->value = value;
    s->wakeups = 0;
    s->waiting = 0;
    if (pthread_mutex_init(&s->mu, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&s->cv, NULL) != 0) {
        pthread_mutex_destroy(&s->mu);
        return -1;
    }
    return 0;
}

void sem_destroy(Sem *s) {
    pthread_cond_destroy(&s->cv);
    pthread_mutex_destroy(&s->mu);
}

void sem_P(Sem *s) {
    pthread_mutex_lock(&s->mu);
    s->value--;
    if (s->value < 0) {
        /* 条件变量可能被虚假叫醒。wakeups 记下还没领走的 V，叫醒一次才能离开一次。 */
        s->waiting++;
        while (s->wakeups == 0) {
            pthread_cond_wait(&s->cv, &s->mu);
        }
        s->wakeups--;
        s->waiting--;
    }
    pthread_mutex_unlock(&s->mu);
}

void sem_V(Sem *s) {
    pthread_mutex_lock(&s->mu);
    s->value++;
    if (s->value <= 0) {
        s->wakeups++;
        pthread_cond_signal(&s->cv);
    }
    pthread_mutex_unlock(&s->mu);
}

void sem_snapshot(Sem *s, int *value, int *waiting) {
    pthread_mutex_lock(&s->mu);
    *value = s->value;
    *waiting = s->waiting;
    pthread_mutex_unlock(&s->mu);
}
