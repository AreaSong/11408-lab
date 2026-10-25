#ifndef THREADS_H
#define THREADS_H

typedef struct {
    int produced;
    int consumed;
    int sum;
    int overflow;
} ThreadPc;

typedef struct {
    int reads;
    int writes;
    int overlap;
} ThreadRw;

typedef struct {
    int eaten;
    int bad;
} ThreadPhi;

typedef struct {
    int smoked;
    int glue;
    int paper;
    int tobacco;
} ThreadSmoke;

typedef struct {
    int watered;
    int max_open;
    int bad;
} ThreadPlant;

typedef struct {
    int eaten;
    int bad;
    int max_users;
} ThreadBowl;

typedef struct {
    int empty_waiters;
    int mutex_waiters;
} ThreadWrong;

enum { THREAD_PHI_LIMIT = 1, THREAD_PHI_ODD = 2 };

int thread_pc(ThreadPc *out);
int thread_rw(ThreadRw *out);
int thread_phi(int scheme, ThreadPhi *out);
int thread_smoke(ThreadSmoke *out);
int thread_plant(ThreadPlant *out);
int thread_bowl(ThreadBowl *out);
int thread_wrong(ThreadWrong *out);

#endif
