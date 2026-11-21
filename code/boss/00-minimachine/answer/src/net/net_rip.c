#include "net/net_rip.h"

#include <stdio.h>
#include <string.h>

static int offer_of(int dist) {
    return dist >= 15 ? 16 : dist + 1;
}

static void take(int *dist, char *via, int cap, int offer, const char *from, int must) {
    if (!must && offer >= *dist) {
        return;
    }
    *dist = offer;
    snprintf(via, (size_t)cap, "%s", offer >= 16 ? "-" : from);
}

int net_rip_rules(char row[5][32]) {
    static const char *net[] = {"Net1", "Net2", "Net3", "Net4", "Net5"};
    int dist[] = {1, 3, 4, 5, 16};
    char via[5][8] = {"直连", "R2", "R3", "R2", "-"};
    int adv[] = {2, 1, 2, 6, 3};
    int i;
    if (row == NULL) {
        return -1;
    }
    for (i = 0; i < 5; i++) {
        int from_r2 = strcmp(via[i], "R2") == 0;
        int missing = dist[i] >= 16;
        take(&dist[i], via[i], (int)sizeof via[i], offer_of(adv[i]), "R2", from_r2 || missing);
        snprintf(row[i], 32, "%s %d %s", net[i], dist[i], via[i]);
    }
    return 5;
}

int net_rip_2016(int *dist, char *via, int cap) {
    int from_r1 = offer_of(2);
    int from_r3 = offer_of(16);
    if (dist == NULL || via == NULL) {
        return -1;
    }
    if (from_r1 <= from_r3) {
        *dist = from_r1;
        snprintf(via, (size_t)cap, "R1");
    } else {
        *dist = from_r3;
        snprintf(via, (size_t)cap, "R3");
    }
    return 0;
}

typedef struct {
    int dist;
    int via;
} RipNode;

static void deliver(RipNode *dst, int dst_id, const RipNode *src, int src_id, int split) {
    int offer = (split && src->via == dst_id) ? 16 : offer_of(src->dist);
    int from_src = dst->via == src_id;
    if (from_src || dst->dist >= 16 || offer < dst->dist) {
        dst->dist = offer;
        dst->via = offer >= 16 ? 0 : src_id;
    }
}

int net_rip_slow(int split, int *rounds) {
    RipNode a = {16, 0};
    RipNode b = {2, 1};
    int round = 0;
    if (rounds == NULL) {
        return -1;
    }
    while (round < 40) {
        round++;
        if (round % 2 == 1) {
            deliver(&a, 1, &b, 2, split);
        } else {
            deliver(&b, 2, &a, 1, split);
        }
        if (a.dist >= 16 && b.dist >= 16) {
            *rounds = round;
            return 0;
        }
    }
    return -1;
}

static const int k_edge[][2] = {{0, 1}, {0, 3}, {1, 2}, {2, 5}, {2, 4}, {5, 4}, {4, 3}};

static int linked(int a, int b) {
    int i;
    for (i = 0; i < 7; i++) {
        if ((k_edge[i][0] == a && k_edge[i][1] == b) || (k_edge[i][0] == b && k_edge[i][1] == a)) {
            return 1;
        }
    }
    return 0;
}

static void spread(const int *old_d, const int *old_via, int *dist, int *via) {
    int v;
    int u;
    for (v = 0; v < 6; v++) {
        dist[v] = old_d[v];
        via[v] = old_via[v];
    }
    for (v = 0; v < 6; v++) {
        if (old_via[v] == -2) {
            continue;
        }
        for (u = 0; u < 6; u++) {
            int offer;
            if (!linked(u, v) || old_d[u] >= 16) {
                continue;
            }
            offer = old_d[u] + 1;
            if (offer < dist[v] || (offer == dist[v] && (via[v] < 0 || u < via[v]))) {
                dist[v] = offer;
                via[v] = u;
            }
        }
    }
}

static int converge(int origin, int *dist, int *via, int *seconds) {
    int cur_d[6];
    int cur_via[6];
    int next_d[6];
    int next_via[6];
    int t = 0;
    int round;
    int i;
    for (i = 0; i < 6; i++) {
        cur_d[i] = 16;
        cur_via[i] = -1;
    }
    cur_d[origin] = 1;
    cur_via[origin] = -2;
    for (round = 0; round < 8; round++) {
        int done = 1;
        spread(cur_d, cur_via, next_d, next_via);
        memcpy(cur_d, next_d, sizeof cur_d);
        memcpy(cur_via, next_via, sizeof cur_via);
        for (i = 0; i < 6; i++) {
            if (cur_d[i] >= 16) {
                done = 0;
            }
        }
        if (done) {
            if (dist != NULL) {
                memcpy(dist, cur_d, sizeof cur_d);
            }
            if (via != NULL) {
                memcpy(via, cur_via, sizeof cur_via);
            }
            if (seconds != NULL) {
                *seconds = t;
            }
            return 0;
        }
        t += 30;
    }
    return -1;
}

int net_rip_when(int origin, int *seconds) {
    if (origin < 0 || origin > 5) {
        return -1;
    }
    return converge(origin, NULL, NULL, seconds);
}

int net_rip_via(int origin, int node, int *dist, int *via) {
    int all_d[6];
    int all_via[6];
    if (node < 0 || node > 5 || dist == NULL || via == NULL) {
        return -1;
    }
    if (converge(origin, all_d, all_via, NULL) != 0) {
        return -1;
    }
    *dist = all_d[node];
    *via = all_via[node];
    return 0;
}

static int line_rules(char *out, int cap) {
    char row[5][32];
    int n;
    if (net_rip_rules(row) != 5) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s; %s; %s; %s; %s", row[0], row[1], row[2], row[3], row[4]);
    return n > 0 && n < cap ? 0 : -1;
}

int net_rip_line(const char *id, char *out, int cap) {
    int rounds = 0;
    int dist = 0;
    int t4 = 0;
    int t3 = 0;
    char via[8];
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "rules") == 0) {
        return line_rules(out, cap);
    }
    if (strcmp(id, "2016-37") == 0) {
        if (net_rip_2016(&dist, via, (int)sizeof via) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "dist=%d via=%s", dist, via);
        return n > 0 && n < cap ? 0 : -1;
    }
    if (strcmp(id, "slow") == 0 || strcmp(id, "split") == 0) {
        if (net_rip_slow(strcmp(id, "split") == 0, &rounds) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "rounds=%d", rounds);
        return n > 0 && n < cap ? 0 : -1;
    }
    if (strcmp(id, "2024-47") == 0) {
        if (net_rip_when(5, &t4) != 0 || net_rip_when(3, &t3) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "proto=OSPF ttl=16 t4=%d t3=%d", t4, t3);
        return n > 0 && n < cap ? 0 : -1;
    }
    return -1;
}
