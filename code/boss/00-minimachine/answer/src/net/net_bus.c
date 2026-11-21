#include "net/net_bus.h"

#include <string.h>

/* 和 net/05 同一个教学模型：时隙就是争用期，退避用固定公式，所以每次结果一样。 */

#define NET_BUS_MAX 32

typedef struct {
    int left;
    int defer;
    int collide;
} Sta;

static int bus_pick(int id, int collisions) {
    int k = collisions < 10 ? collisions : 10;
    int mod = 1 << k;
    return (17 * id + 29 * collisions + 3) % mod;
}

static int busy(const Sta *s, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (s[i].left > 0) {
            return 1;
        }
    }
    return 0;
}

static int ready(const Sta *s, int n, int *who) {
    int i;
    int count = 0;
    for (i = 0; i < n; i++) {
        if (s[i].left > 0 && s[i].defer == 0) {
            who[count++] = i;
        }
    }
    return count;
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

static void collide(Sta *s, const int *who, int nwho, int *time) {
    int j;
    for (j = 0; j < nwho; j++) {
        int id = who[j];
        s[id].collide++;
        if (s[id].collide >= 16) {
            s[id].left--;
            s[id].collide = 0;
            s[id].defer = 0;
        } else {
            s[id].defer = bus_pick(id, s[id].collide);
        }
    }
    (*time)++;
}

static void succeed(Sta *s, int n, int id, int length, int *time, long long *useful) {
    int who[1];
    who[0] = id;
    s[id].left--;
    s[id].collide = 0;
    s[id].defer = 0;
    dec_others(s, n, who, 1, length);
    *time += length;
    *useful += length;
}

static int args_ok(int n, int frames, int length) {
    return n >= 1 && n <= NET_BUS_MAX && frames >= 1 && length >= 1;
}

int net_bus_csma(int stations, int frames, int length, long long *useful, long long *total) {
    Sta s[NET_BUS_MAX];
    int who[NET_BUS_MAX];
    int time = 0;
    int i;
    if (!args_ok(stations, frames, length) || useful == NULL || total == NULL) {
        return -1;
    }
    memset(s, 0, sizeof s);
    for (i = 0; i < stations; i++) {
        s[i].left = frames;
    }
    *useful = 0;
    while (busy(s, stations)) {
        int nwho;
        if (time > 1000000) {
            return -1;
        }
        nwho = ready(s, stations, who);
        if (nwho == 0) {
            for (i = 0; i < stations; i++) {
                if (s[i].left > 0 && s[i].defer > 0) {
                    s[i].defer--;
                }
            }
            time++;
            continue;
        }
        if (nwho == 1) {
            succeed(s, stations, who[0], length, &time, useful);
            continue;
        }
        dec_others(s, stations, who, nwho, 1);
        collide(s, who, nwho, &time);
    }
    *total = time;
    return 0;
}

int net_bus_token(int stations, int frames, int length, int walk, long long *useful, long long *total) {
    Sta s[NET_BUS_MAX];
    int token = 0;
    int time = 0;
    int steps = 0;
    int i;
    if (!args_ok(stations, frames, length) || walk < 0 || useful == NULL || total == NULL) {
        return -1;
    }
    memset(s, 0, sizeof s);
    for (i = 0; i < stations; i++) {
        s[i].left = frames;
    }
    *useful = 0;
    while (busy(s, stations)) {
        if (++steps > 1000000) {
            return -1;
        }
        if (s[token].left > 0) {
            s[token].left--;
            time += length;
            *useful += length;
            if (!busy(s, stations)) {
                break;
            }
            time += walk;
        } else {
            time += walk;
        }
        token = (token + 1) % stations;
    }
    *total = time;
    return 0;
}
