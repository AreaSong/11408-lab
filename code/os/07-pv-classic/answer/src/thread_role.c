#include "threads.h"

#include "sem.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

enum { SCH_BOWL = 3 };

typedef struct {
    int id;
    int n;
    int meals;
    int scheme;
    int limit;
    Sem *room;
    Sem *chop;
    atomic_int *eating;
    atomic_int *bad;
    atomic_int *eaten;
    atomic_int *users;
    atomic_int *max_users;
} PhiArg;

static void remember_max(atomic_int *max_users, int now) {
    int old = atomic_load(max_users);
    while (now > old && !atomic_compare_exchange_weak(max_users, &old, now)) {
    }
}

static void *phi_one(void *arg) {
    PhiArg *a = arg;
    int left = a->id;
    int right = (a->id + 1) % a->n;
    int prev = (a->id + a->n - 1) % a->n;
    int next = right;
    int k;
    for (k = 0; k < a->meals; k++) {
        if (a->scheme != THREAD_PHI_ODD) {
            sem_P(a->room);
        }
        if (a->scheme == THREAD_PHI_ODD && a->id % 2 == 0) {
            sem_P(&a->chop[right]);
            sem_P(&a->chop[left]);
        } else {
            sem_P(&a->chop[left]);
            sem_P(&a->chop[right]);
        }
        if (a->limit > 0) {
            int now = atomic_fetch_add(a->users, 1) + 1;
            remember_max(a->max_users, now);
            if (now > a->limit) {
                atomic_store(a->bad, 1);
            }
        }
        if (atomic_load(&a->eating[prev]) || atomic_load(&a->eating[next])) {
            atomic_store(a->bad, 1);
        }
        atomic_store(&a->eating[a->id], 1);
        atomic_store(&a->eating[a->id], 0);
        if (a->limit > 0) {
            atomic_fetch_sub(a->users, 1);
        }
        sem_V(&a->chop[left]);
        sem_V(&a->chop[right]);
        if (a->scheme != THREAD_PHI_ODD) {
            sem_V(a->room);
        }
        atomic_fetch_add(a->eaten, 1);
    }
    return NULL;
}

static int run_phi(int scheme, int limit, int meals, ThreadPhi *out, int *max_users) {
    enum { N = 5 };
    Sem chop[N];
    Sem room;
    PhiArg arg[N];
    pthread_t tid[N];
    atomic_int eating[N];
    atomic_int bad = 0, eaten = 0, users = 0, peak = 0;
    int i;
    int room_value = scheme == SCH_BOWL ? limit : 4;
    for (i = 0; i < N; i++) {
        atomic_store(&eating[i], 0);
        if (sem_init(&chop[i], 1) != 0) {
            return -1;
        }
        arg[i].id = i;
        arg[i].n = N;
        arg[i].meals = meals;
        arg[i].scheme = scheme;
        arg[i].limit = limit;
        arg[i].room = &room;
        arg[i].chop = chop;
        arg[i].eating = eating;
        arg[i].bad = &bad;
        arg[i].eaten = &eaten;
        arg[i].users = &users;
        arg[i].max_users = &peak;
    }
    if (scheme != THREAD_PHI_ODD && sem_init(&room, room_value) != 0) {
        return -1;
    }
    for (i = 0; i < N; i++) {
        if (pthread_create(&tid[i], NULL, phi_one, &arg[i]) != 0) {
            return -1;
        }
    }
    for (i = 0; i < N; i++) {
        pthread_join(tid[i], NULL);
        sem_destroy(&chop[i]);
    }
    if (scheme != THREAD_PHI_ODD) {
        sem_destroy(&room);
    }
    out->eaten = atomic_load(&eaten);
    out->bad = atomic_load(&bad);
    if (max_users != NULL) {
        *max_users = atomic_load(&peak);
    }
    return 0;
}

int thread_phi(int scheme, ThreadPhi *out) {
    int limit = scheme == THREAD_PHI_ODD ? 0 : 4;
    return run_phi(scheme, limit, 2, out, NULL);
}

typedef struct {
    Sem offer[3];
    Sem finish;
    atomic_int got[3];
} SmokeBox;

typedef struct {
    SmokeBox *box;
    int id;
} SmokeArg;

static const int SMOKE_SEQ[] = {0, 1, 2, 0, 1, 2};

static void *smoke_agent(void *arg) {
    SmokeBox *box = arg;
    int i;
    for (i = 0; i < 6; i++) {
        sem_V(&box->offer[SMOKE_SEQ[i]]);
        sem_P(&box->finish);
    }
    return NULL;
}

static void *smoke_one(void *arg) {
    SmokeArg *a = arg;
    int k;
    for (k = 0; k < 2; k++) {
        sem_P(&a->box->offer[a->id]);
        atomic_fetch_add(&a->box->got[a->id], 1);
        sem_V(&a->box->finish);
    }
    return NULL;
}

int thread_smoke(ThreadSmoke *out) {
    SmokeBox box;
    SmokeArg arg[3];
    pthread_t agent;
    pthread_t smoker[3];
    int i;
    memset(&box, 0, sizeof box);
    for (i = 0; i < 3; i++) {
        if (sem_init(&box.offer[i], 0) != 0) {
            return -1;
        }
    }
    if (sem_init(&box.finish, 0) != 0) {
        return -1;
    }
    if (pthread_create(&agent, NULL, smoke_agent, &box) != 0) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        arg[i].box = &box;
        arg[i].id = i;
        if (pthread_create(&smoker[i], NULL, smoke_one, &arg[i]) != 0) {
            return -1;
        }
    }
    pthread_join(agent, NULL);
    for (i = 0; i < 3; i++) {
        pthread_join(smoker[i], NULL);
        sem_destroy(&box.offer[i]);
    }
    sem_destroy(&box.finish);
    out->glue = atomic_load(&box.got[0]);
    out->paper = atomic_load(&box.got[1]);
    out->tobacco = atomic_load(&box.got[2]);
    out->smoked = out->glue + out->paper + out->tobacco;
    return 0;
}

typedef struct {
    Sem pits;
    Sem empty;
    Sem water;
    Sem mutex;
    int trees;
    int open;
    int max_open;
    int watered;
    int bad;
} PlantBox;

static void *plant_jia(void *arg) {
    PlantBox *box = arg;
    int i;
    for (i = 0; i < box->trees; i++) {
        sem_P(&box->pits);
        sem_P(&box->mutex);
        box->open++;
        if (box->open > box->max_open) {
            box->max_open = box->open;
        }
        if (box->open > 3) {
            box->bad = 1;
        }
        sem_V(&box->mutex);
        sem_V(&box->empty);
    }
    return NULL;
}

static void *plant_yi(void *arg) {
    PlantBox *box = arg;
    int i;
    for (i = 0; i < box->trees; i++) {
        sem_P(&box->empty);
        sem_P(&box->mutex);
        box->open--;
        if (box->open < 0) {
            box->bad = 1;
        }
        sem_V(&box->mutex);
        sem_V(&box->pits);
        sem_V(&box->water);
    }
    return NULL;
}

static void *plant_bing(void *arg) {
    PlantBox *box = arg;
    int i;
    for (i = 0; i < box->trees; i++) {
        sem_P(&box->water);
        box->watered++;
    }
    return NULL;
}

int thread_plant(ThreadPlant *out) {
    PlantBox box;
    pthread_t jia;
    pthread_t yi;
    pthread_t bing;
    memset(&box, 0, sizeof box);
    box.trees = 5;
    if (sem_init(&box.pits, 3) != 0 || sem_init(&box.empty, 0) != 0 ||
        sem_init(&box.water, 0) != 0 || sem_init(&box.mutex, 1) != 0) {
        return -1;
    }
    if (pthread_create(&jia, NULL, plant_jia, &box) != 0 ||
        pthread_create(&yi, NULL, plant_yi, &box) != 0 ||
        pthread_create(&bing, NULL, plant_bing, &box) != 0) {
        return -1;
    }
    pthread_join(jia, NULL);
    pthread_join(yi, NULL);
    pthread_join(bing, NULL);
    out->watered = box.watered;
    out->max_open = box.max_open;
    out->bad = box.bad;
    sem_destroy(&box.pits);
    sem_destroy(&box.empty);
    sem_destroy(&box.water);
    sem_destroy(&box.mutex);
    return 0;
}

int thread_bowl(ThreadBowl *out) {
    ThreadPhi phi;
    int peak = 0;
    if (run_phi(SCH_BOWL, 2, 1, &phi, &peak) != 0) {
        return -1;
    }
    out->eaten = phi.eaten;
    out->bad = phi.bad;
    out->max_users = peak;
    return 0;
}

typedef struct {
    Sem empty;
    Sem mutex;
} WrongBox;

static void *wrong_producer(void *arg) {
    WrongBox *box = arg;
    sem_P(&box->mutex);
    sem_P(&box->empty);
    sem_V(&box->mutex);
    return NULL;
}

static void *wrong_consumer(void *arg) {
    WrongBox *box = arg;
    sem_P(&box->mutex);
    sem_V(&box->mutex);
    return NULL;
}

static int wait_blocked(Sem *sem) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        struct timespec now;
        int value = 0;
        int waiting = 0;
        sem_snapshot(sem, &value, &waiting);
        if (waiting >= 1) {
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 2) {
            return -1;
        }
        sched_yield();
    }
}

int thread_wrong(ThreadWrong *out) {
    WrongBox box;
    pthread_t producer;
    pthread_t consumer;
    int empty_value = 0;
    int mutex_value = 0;
    if (sem_init(&box.empty, 0) != 0 || sem_init(&box.mutex, 1) != 0) {
        return -1;
    }
    if (pthread_create(&producer, NULL, wrong_producer, &box) != 0) {
        return -1;
    }
    if (wait_blocked(&box.empty) != 0) {
        return -1;
    }
    if (pthread_create(&consumer, NULL, wrong_consumer, &box) != 0) {
        return -1;
    }
    if (wait_blocked(&box.mutex) != 0) {
        return -1;
    }
    sem_snapshot(&box.empty, &empty_value, &out->empty_waiters);
    sem_snapshot(&box.mutex, &mutex_value, &out->mutex_waiters);
    /* 叫醒生产者只为让进程能退出，不属于错误算法。 */
    sem_V(&box.empty);
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    sem_destroy(&box.empty);
    sem_destroy(&box.mutex);
    (void)empty_value;
    (void)mutex_value;
    return 0;
}
