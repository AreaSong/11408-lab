#ifndef SPIN_H
#define SPIN_H

#include <stdatomic.h>

typedef struct {
    atomic_flag flag;
} Tsl;

typedef struct {
    atomic_int lock;
} SwapLock;

void tsl_lock(Tsl *l);
void tsl_unlock(Tsl *l);
void swap_init(SwapLock *l);
void swap_lock(SwapLock *l);
void swap_unlock(SwapLock *l);

#endif
