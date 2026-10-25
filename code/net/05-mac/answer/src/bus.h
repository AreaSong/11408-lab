#ifndef BUS_H
#define BUS_H

#include <stdbool.h>

#define BUS_MAX 32
#define BUS_TRACE_MAX 256

typedef enum {
    EV_COLLIDE = 1,
    EV_SEND,
    EV_WAIT,
    EV_TOKEN_SEND,
    EV_TOKEN_WALK,
    EV_TOKEN_SKIP
} EvKind;

typedef struct {
    EvKind kind;
    int time;
    int length;
    int nwho;
    int id[BUS_MAX];
    int coll[BUS_MAX];
    int k[BUS_MAX];
    int pick[BUS_MAX];
    int rmax[BUS_MAX];
    int drop[BUS_MAX];
} BusEvent;

typedef struct {
    BusEvent *ev;
    int cap;
    int n;
    int cut;
} BusTrace;

typedef struct {
    long long useful;
    long long total;
    int collisions;
    int dropped;
    int done;
} BusStat;

/* 模拟器里的“随机”时隙。真题只考范围，这里用固定公式，同一输入每次相同。 */
int bus_pick(int id, int collisions, int *k, int *rmax);
int bus_run_csma(int n, int frames, int length, BusTrace *tr, BusStat *st);
int bus_run_token(int n, int frames, int length, int walk, BusTrace *tr, BusStat *st);

#endif
