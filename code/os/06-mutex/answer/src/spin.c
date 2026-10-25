#include "spin.h"

void tsl_lock(Tsl *l) {
    /* 返回值是旧标志。旧值已经是 true，说明别人占着，自己继续转。这是忙等。 */
    while (atomic_flag_test_and_set_explicit(&l->flag, memory_order_acquire)) {
    }
}

void tsl_unlock(Tsl *l) {
    atomic_flag_clear_explicit(&l->flag, memory_order_release);
}

void swap_init(SwapLock *l) {
    atomic_init(&l->lock, 0);
}

void swap_lock(SwapLock *l) {
    int key = 1;
    while (key) {
        key = atomic_exchange_explicit(&l->lock, key, memory_order_acquire);
    }
}

void swap_unlock(SwapLock *l) {
    atomic_store_explicit(&l->lock, 0, memory_order_release);
}
