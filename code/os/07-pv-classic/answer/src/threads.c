#include "threads.h"

#include "sem.h"

#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

typedef struct {
    Sem empty;
    Sem full;
    Sem mutex;
    int buf[8];
    int cap;
    int head;
    int tail;
    int count;
    int items;
    int produced;
    int consumed;
    int sum_in;
    int sum_out;
    int overflow;
} PcBox;

static void *pc_producer(void *arg) {
    PcBox *box = arg;
    int i;
    for (i = 1; i <= box->items; i++) {
        sem_P(&box->empty);
        sem_P(&box->mutex);
        if (box->count >= box->cap) {
            box->overflow = 1;
        }
        box->buf[box->head] = i;
        box->head = (box->head + 1) % box->cap;
        box->count++;
        box->sum_in += i;
        box->produced++;
        sem_V(&box->mutex);
        sem_V(&box->full);
    }
    return NULL;
}

static void *pc_consumer(void *arg) {
    PcBox *box = arg;
    int i;
    for (i = 0; i < box->items; i++) {
        int got;
        sem_P(&box->full);
        sem_P(&box->mutex);
        got = box->buf[box->tail];
        box->tail = (box->tail + 1) % box->cap;
        box->count--;
        box->sum_out += got;
        box->consumed++;
        sem_V(&box->mutex);
        sem_V(&box->empty);
    }
    return NULL;
}

int thread_pc(ThreadPc *out) {
    PcBox box;
    pthread_t producer;
    pthread_t consumer;
    memset(&box, 0, sizeof box);
    box.cap = 3;
    box.items = 20;
    if (sem_init(&box.empty, box.cap) != 0 || sem_init(&box.full, 0) != 0 ||
        sem_init(&box.mutex, 1) != 0) {
        return -1;
    }
    if (pthread_create(&producer, NULL, pc_producer, &box) != 0 ||
        pthread_create(&consumer, NULL, pc_consumer, &box) != 0) {
        return -1;
    }
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    out->produced = box.produced;
    out->consumed = box.consumed;
    out->sum = box.sum_out;
    out->overflow = box.overflow || box.sum_in != box.sum_out;
    sem_destroy(&box.empty);
    sem_destroy(&box.full);
    sem_destroy(&box.mutex);
    return 0;
}

typedef struct {
    Sem rmutex;
    Sem wmutex;
    int readcount;
    atomic_int reading;
    atomic_int writing;
    atomic_int bad;
    atomic_int reads;
    atomic_int writes;
    int times;
} RwBox;

static void *rw_reader(void *arg) {
    RwBox *box = arg;
    int k;
    for (k = 0; k < box->times; k++) {
        sem_P(&box->rmutex);
        box->readcount++;
        if (box->readcount == 1) {
            sem_P(&box->wmutex);
        }
        sem_V(&box->rmutex);
        atomic_fetch_add(&box->reading, 1);
        if (atomic_load(&box->writing) > 0) {
            atomic_store(&box->bad, 1);
        }
        atomic_fetch_sub(&box->reading, 1);
        sem_P(&box->rmutex);
        box->readcount--;
        if (box->readcount == 0) {
            sem_V(&box->wmutex);
        }
        sem_V(&box->rmutex);
        atomic_fetch_add(&box->reads, 1);
    }
    return NULL;
}

static void *rw_writer(void *arg) {
    RwBox *box = arg;
    int k;
    for (k = 0; k < box->times; k++) {
        sem_P(&box->wmutex);
        if (atomic_load(&box->reading) > 0 || atomic_load(&box->writing) > 0) {
            atomic_store(&box->bad, 1);
        }
        atomic_fetch_add(&box->writing, 1);
        atomic_fetch_sub(&box->writing, 1);
        sem_V(&box->wmutex);
        atomic_fetch_add(&box->writes, 1);
    }
    return NULL;
}

int thread_rw(ThreadRw *out) {
    RwBox box;
    pthread_t readers[3];
    pthread_t writers[2];
    int i;
    memset(&box, 0, sizeof box);
    box.times = 2;
    if (sem_init(&box.rmutex, 1) != 0 || sem_init(&box.wmutex, 1) != 0) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (pthread_create(&readers[i], NULL, rw_reader, &box) != 0) {
            return -1;
        }
    }
    for (i = 0; i < 2; i++) {
        if (pthread_create(&writers[i], NULL, rw_writer, &box) != 0) {
            return -1;
        }
    }
    for (i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }
    for (i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }
    out->reads = atomic_load(&box.reads);
    out->writes = atomic_load(&box.writes);
    out->overlap = atomic_load(&box.bad);
    sem_destroy(&box.rmutex);
    sem_destroy(&box.wmutex);
    return 0;
}

