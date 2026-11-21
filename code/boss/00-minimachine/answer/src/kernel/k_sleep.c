#include "kernel/k_i.h"

int k_timer_push(KWorld *w, uint32_t when, int pid) {
    int key;
    /* 小根堆只比较整数。到期时刻放高位，同一时刻 pid 小的先醒。 */
    if (pid < 0 || pid >= 16 || when > 100000000u) {
        return -1;
    }
    key = (int)when * 16 + pid;
    return heap_push(&w->sleepers, key, NULL) == HEAP_OK ? 0 : -1;
}

int k_timer_due(KWorld *w, uint32_t now, int *pid) {
    int key = 0;
    uint32_t when;
    if (w->sleepers.n <= 0) {
        return 0;
    }
    when = (uint32_t)(w->sleepers.key[1] / 16);
    if (when > now) {
        return 0;
    }
    if (heap_pop(&w->sleepers, &key, NULL) != HEAP_OK) {
        return 0;
    }
    *pid = key % 16;
    return 1;
}
