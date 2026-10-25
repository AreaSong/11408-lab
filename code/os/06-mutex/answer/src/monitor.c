#include "monitor.h"

int monitor_init(Monitor *m, int cap) {
    if (cap < 1 || cap > MONITOR_CAP) {
        return -1;
    }
    m->cap = cap;
    m->count = 0;
    m->in = 0;
    m->out = 0;
    if (pthread_mutex_init(&m->mu, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&m->not_full, NULL) != 0) {
        pthread_mutex_destroy(&m->mu);
        return -1;
    }
    if (pthread_cond_init(&m->not_empty, NULL) != 0) {
        pthread_cond_destroy(&m->not_full);
        pthread_mutex_destroy(&m->mu);
        return -1;
    }
    return 0;
}

void monitor_destroy(Monitor *m) {
    pthread_cond_destroy(&m->not_empty);
    pthread_cond_destroy(&m->not_full);
    pthread_mutex_destroy(&m->mu);
}

void monitor_put(Monitor *m, int item) {
    pthread_mutex_lock(&m->mu);
    /* pthread 是 Mesa：signal 之后自己还握着锁，被叫醒的人要重新看条件。不能写成 if。 */
    while (m->count == m->cap) {
        pthread_cond_wait(&m->not_full, &m->mu);
    }
    m->buf[m->in] = item;
    m->in = (m->in + 1) % m->cap;
    m->count++;
    pthread_cond_signal(&m->not_empty);
    pthread_mutex_unlock(&m->mu);
}

int monitor_get(Monitor *m) {
    int item;
    pthread_mutex_lock(&m->mu);
    while (m->count == 0) {
        pthread_cond_wait(&m->not_empty, &m->mu);
    }
    item = m->buf[m->out];
    m->out = (m->out + 1) % m->cap;
    m->count--;
    pthread_cond_signal(&m->not_full);
    pthread_mutex_unlock(&m->mu);
    return item;
}
