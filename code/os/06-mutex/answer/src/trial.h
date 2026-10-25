#ifndef TRIAL_H
#define TRIAL_H

enum {
    TRIAL_TSL = 0,
    TRIAL_SWAP,
    TRIAL_MUTEX,
    TRIAL_SEM
};

int trial_bare(int n, int *got);
int trial_lock(int kind, int n, int waste, int *got);

#endif
