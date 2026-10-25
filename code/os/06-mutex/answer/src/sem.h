#ifndef SEM_H
#define SEM_H

#include <pthread.h>

/*
 * 记录型信号量。macOS 没有 sem_init，用互斥锁和条件变量自己做 P/V。
 * value 的含义与王道相同：可以为负，绝对值是还没被叫醒的等待数。
 */
typedef struct {
    int value;
    int wakeups;
    int waiting;
    pthread_mutex_t mu;
    pthread_cond_t cv;
} Sem;

int sem_init(Sem *s, int value);
void sem_destroy(Sem *s);
void sem_P(Sem *s);
void sem_V(Sem *s);
void sem_snapshot(Sem *s, int *value, int *waiting);

#endif
