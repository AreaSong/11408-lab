#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>

#define MONITOR_CAP 8

/* 管程：外面只能 put / get。互斥和两个条件变量都关在结构体里面。 */
typedef struct {
    int buf[MONITOR_CAP];
    int cap;
    int count;
    int in;
    int out;
    pthread_mutex_t mu;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} Monitor;

int monitor_init(Monitor *m, int cap);
void monitor_destroy(Monitor *m);
void monitor_put(Monitor *m, int item);
int monitor_get(Monitor *m);

#endif
