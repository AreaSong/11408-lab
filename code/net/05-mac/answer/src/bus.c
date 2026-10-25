#include "bus.h"

#include <string.h>

/*
 * 教学模型，不是网卡里的完整状态机。
 * 时隙就是争用期：冲突在这一段里就能发现，所以只浪费 1 个时隙，不会把整帧发完。
 * 1-坚持：退避到 0 立刻抢。退避在别人发送时照样倒数，倒数到 0 就等信道空。
 * 令牌环没有冲突。持有令牌才发，发完一帧就交出去。
 */

int bus_pick(int id, int collisions, int *k, int *rmax) {
    int kk;
    int mod;
    int r;

    if (id < 0 || collisions <= 0 || collisions >= 16) {
        return -1;
    }
    kk = collisions < 10 ? collisions : 10;
    mod = 1 << kk;
    r = (17 * id + 29 * collisions + 3) % mod;
    if (k != NULL) {
        *k = kk;
    }
    if (rmax != NULL) {
        *rmax = mod - 1;
    }
    return r;
}

typedef struct {
    int left;
    int defer;
    int collide;
} Sta;

static int busy(const Sta *s, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (s[i].left > 0) {
            return 1;
        }
    }
    return 0;
}

static void arm(Sta *s, int n, int frames) {
    int i;

    for (i = 0; i < n; i++) {
        s[i].left = frames;
        s[i].defer = 0;
        s[i].collide = 0;
    }
}

static int ready_ids(const Sta *s, int n, int *who) {
    int i;
    int count = 0;

    for (i = 0; i < n; i++) {
        if (s[i].left > 0 && s[i].defer == 0) {
            who[count++] = i;
        }
    }
    return count;
}

static void push_ev(BusTrace *tr, const BusEvent *ev) {
    if (tr == NULL) {
        return;
    }
    if (tr->n >= tr->cap) {
        tr->cut = 1;
        return;
    }
    tr->ev[tr->n++] = *ev;
}

static int listed(const int *who, int nwho, int id) {
    int i;

    for (i = 0; i < nwho; i++) {
        if (who[i] == id) {
            return 1;
        }
    }
    return 0;
}

static void dec_others(Sta *s, int n, const int *who, int nwho, int slots) {
    int i;

    for (i = 0; i < n; i++) {
        if (s[i].left <= 0 || s[i].defer <= 0 || listed(who, nwho, i)) {
            continue;
        }
        s[i].defer -= slots;
        if (s[i].defer < 0) {
            s[i].defer = 0;
        }
    }
}

static void decay(Sta *s, int n) {
    int i;

    for (i = 0; i < n; i++) {
        if (s[i].left > 0 && s[i].defer > 0) {
            s[i].defer--;
        }
    }
}

static void note_collide(Sta *s, BusStat *st, BusEvent *ev, int id, int slot) {
    int now;

    s[id].collide++;
    now = s[id].collide;
    ev->id[slot] = id;
    ev->coll[slot] = now;
    if (now >= 16) {
        ev->drop[slot] = 1;
        ev->pick[slot] = -1;
        s[id].left--;
        s[id].collide = 0;
        s[id].defer = 0;
        st->dropped++;
        return;
    }
    ev->pick[slot] = bus_pick(id, now, &ev->k[slot], &ev->rmax[slot]);
    s[id].defer = ev->pick[slot];
}

static void collide(Sta *s, int n, const int *who, int nwho, int *time, BusTrace *tr, BusStat *st) {
    BusEvent ev;
    int j;

    memset(&ev, 0, sizeof ev);
    ev.kind = EV_COLLIDE;
    ev.time = *time;
    ev.nwho = nwho;
    for (j = 0; j < nwho; j++) {
        note_collide(s, st, &ev, who[j], j);
    }
    dec_others(s, n, who, nwho, 1);
    (*time)++;
    st->collisions++;
    push_ev(tr, &ev);
}

static void succeed(Sta *s, int n, int id, int length, int *time, BusTrace *tr, BusStat *st) {
    BusEvent ev;
    int who[1];

    memset(&ev, 0, sizeof ev);
    ev.kind = EV_SEND;
    ev.time = *time;
    ev.length = length;
    ev.nwho = 1;
    ev.id[0] = id;
    who[0] = id;
    s[id].left--;
    s[id].collide = 0;
    s[id].defer = 0;
    dec_others(s, n, who, 1, length);
    *time += length;
    st->useful += length;
    push_ev(tr, &ev);
}

static int wait_idle(Sta *s, int n, int *time, BusTrace *tr) {
    BusEvent ev;
    int who[BUS_MAX];
    int start = *time;
    int idle = 0;

    memset(&ev, 0, sizeof ev);
    while (busy(s, n) && ready_ids(s, n, who) == 0) {
        decay(s, n);
        (*time)++;
        idle++;
        if (*time > 1000000) {
            return 0;
        }
    }
    ev.kind = EV_WAIT;
    ev.time = start;
    ev.length = idle;
    push_ev(tr, &ev);
    return 1;
}

static int csma_args(int n, int frames, int length, BusStat *st) {
    if (st == NULL || n < 1 || n > BUS_MAX || frames < 1 || length < 1) {
        return 0;
    }
    memset(st, 0, sizeof *st);
    return 1;
}

int bus_run_csma(int n, int frames, int length, BusTrace *tr, BusStat *st) {
    Sta s[BUS_MAX];
    int who[BUS_MAX];
    int time = 0;
    int nwho;

    if (!csma_args(n, frames, length, st)) {
        return 0;
    }
    arm(s, n, frames);
    while (busy(s, n)) {
        if (time > 1000000) {
            st->total = time;
            return 0;
        }
        nwho = ready_ids(s, n, who);
        if (nwho == 0) {
            if (!wait_idle(s, n, &time, tr)) {
                st->total = time;
                return 0;
            }
            continue;
        }
        if (nwho == 1) {
            succeed(s, n, who[0], length, &time, tr, st);
            continue;
        }
        collide(s, n, who, nwho, &time, tr, st);
    }
    st->total = time;
    st->done = 1;
    return 1;
}

static void token_send(Sta *s, int token, int length, int *time, BusTrace *tr, BusStat *st) {
    BusEvent ev;

    memset(&ev, 0, sizeof ev);
    ev.kind = EV_TOKEN_SEND;
    ev.time = *time;
    ev.length = length;
    ev.id[0] = token;
    s[token].left--;
    *time += length;
    st->useful += length;
    push_ev(tr, &ev);
}

static void token_move(int token, int walk, int empty, int *time, BusTrace *tr) {
    BusEvent ev;

    memset(&ev, 0, sizeof ev);
    ev.kind = empty ? EV_TOKEN_SKIP : EV_TOKEN_WALK;
    ev.time = *time;
    ev.length = walk;
    ev.id[0] = token;
    *time += walk;
    push_ev(tr, &ev);
}

int bus_run_token(int n, int frames, int length, int walk, BusTrace *tr, BusStat *st) {
    Sta s[BUS_MAX];
    int token = 0;
    int time = 0;
    int steps = 0;

    if (!csma_args(n, frames, length, st) || walk < 0) {
        return 0;
    }
    arm(s, n, frames);
    while (busy(s, n)) {
        if (++steps > 1000000) {
            st->total = time;
            return 0;
        }
        if (s[token].left > 0) {
            token_send(s, token, length, &time, tr, st);
            if (!busy(s, n)) {
                break;
            }
            token_move(token, walk, 0, &time, tr);
        } else {
            token_move(token, walk, 1, &time, tr);
        }
        token = (token + 1) % n;
    }
    st->total = time;
    st->done = 1;
    return 1;
}
