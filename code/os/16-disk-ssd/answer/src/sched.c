#include "sched.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int track;
    int id;
    int done;
} Item;

static int iabs(int x) {
    return x < 0 ? -x : x;
}

static int in_range(int head, int has_max, int max_track, const int *req, int n) {
    int i;

    if (head < 0 || n < 1 || n > SCHED_N) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (req[i] < 0 || req[i] > 1000000) {
            return 0;
        }
        if (has_max && (req[i] > max_track || head > max_track || max_track < 0)) {
            return 0;
        }
    }
    return 1;
}

static void serve(Item *a, int idx, int *head, int *move, SchedOut *out) {
    *move += iabs(a[idx].track - *head);
    *head = a[idx].track;
    a[idx].done = 1;
    out->track[out->n] = a[idx].track;
    out->index[out->n] = a[idx].id;
    out->n++;
}

/* 当前方向上距离最小；距离相同取先到达的。d = 0 表示磁头已经在这条磁道上。 */
static int nearest_ahead(Item *a, int n, int head, int dir) {
    int best = -1;
    int best_dist = 0;
    int i;

    for (i = 0; i < n; i++) {
        int delta;
        int dist;

        if (a[i].done) {
            continue;
        }
        delta = a[i].track - head;
        dist = dir > 0 ? delta : -delta;
        if (dist < 0) {
            continue;
        }
        if (best < 0 || dist < best_dist || (dist == best_dist && a[i].id < a[best].id)) {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

static int pending_behind(Item *a, int n, int head, int dir) {
    int i;

    for (i = 0; i < n; i++) {
        if (a[i].done) {
            continue;
        }
        if (dir > 0 && a[i].track < head) {
            return 1;
        }
        if (dir < 0 && a[i].track > head) {
            return 1;
        }
    }
    return 0;
}

/* C-LOOK 跳到另一侧最远的那条请求：向上扫时是最小号，向下扫时是最大号。 */
static int extreme_behind(Item *a, int n, int head, int dir) {
    int best = -1;
    int i;

    for (i = 0; i < n; i++) {
        if (a[i].done) {
            continue;
        }
        if (dir > 0 && a[i].track >= head) {
            continue;
        }
        if (dir < 0 && a[i].track <= head) {
            continue;
        }
        if (best < 0) {
            best = i;
            continue;
        }
        if (dir > 0 && a[i].track < a[best].track) {
            best = i;
        }
        if (dir < 0 && a[i].track > a[best].track) {
            best = i;
        }
    }
    return best;
}

static void load_items(Item *a, const int *req, int n) {
    int i;

    for (i = 0; i < n; i++) {
        a[i].track = req[i];
        a[i].id = i;
        a[i].done = 0;
    }
}

static int sweep(Item *a, int n, int head, int dir, int max_track, int to_end, int cyclic,
                 SchedOut *out) {
    int move = 0;
    int guard = 0;

    out->n = 0;
    while (out->n < n && guard < n + 4) {
        int idx = nearest_ahead(a, n, head, dir);
        int end;
        int other;

        guard++;
        if (idx >= 0) {
            serve(a, idx, &head, &move, out);
            continue;
        }
        /* 另一侧已经没有请求就停在最后一次服务处，不再空跑到端点。 */
        if (!pending_behind(a, n, head, dir)) {
            break;
        }
        if (to_end) {
            end = dir > 0 ? max_track : 0;
            move += iabs(end - head);
            head = end;
        }
        if (cyclic && to_end) {
            other = dir > 0 ? 0 : max_track;
            move += iabs(other - head);
            head = other;
        } else if (cyclic) {
            idx = extreme_behind(a, n, head, dir);
            if (idx < 0) {
                return -1;
            }
            move += iabs(a[idx].track - head);
            head = a[idx].track;
        } else {
            dir = -dir;
        }
    }
    if (out->n != n) {
        return -1;
    }
    out->move = move;
    return 0;
}

static int sstf(Item *a, int n, int head, SchedOut *out) {
    int move = 0;
    int left;

    out->n = 0;
    for (left = 0; left < n; left++) {
        int best = -1;
        int best_dist = 0;
        int i;

        for (i = 0; i < n; i++) {
            int dist;

            if (a[i].done) {
                continue;
            }
            dist = iabs(a[i].track - head);
            if (best < 0 || dist < best_dist ||
                (dist == best_dist && a[i].track < a[best].track) ||
                (dist == best_dist && a[i].track == a[best].track && a[i].id < a[best].id)) {
                best = i;
                best_dist = dist;
            }
        }
        serve(a, best, &head, &move, out);
    }
    out->move = move;
    return 0;
}

static int fcfs(Item *a, int n, int head, SchedOut *out) {
    int move = 0;
    int i;

    out->n = 0;
    for (i = 0; i < n; i++) {
        serve(a, i, &head, &move, out);
    }
    out->move = move;
    return 0;
}

int sched_run(const char *algo, int head, int dir, int has_max, int max_track,
              const int *req, int n, SchedOut *out) {
    Item items[SCHED_N];
    int to_end;
    int cyclic;

    if (!in_range(head, has_max, max_track, req, n)) {
        return -1;
    }
    load_items(items, req, n);
    if (strcmp(algo, "fcfs") == 0) {
        return fcfs(items, n, head, out);
    }
    if (strcmp(algo, "sstf") == 0) {
        return sstf(items, n, head, out);
    }
    to_end = strcmp(algo, "scan") == 0 || strcmp(algo, "cscan") == 0;
    cyclic = strcmp(algo, "cscan") == 0 || strcmp(algo, "clook") == 0;
    if (!to_end && !cyclic && strcmp(algo, "look") != 0) {
        return -1;
    }
    if (dir == 0 || (to_end && !has_max)) {
        return -1;
    }
    return sweep(items, n, head, dir, max_track, to_end, cyclic, out);
}

void sched_print(const char *algo, const SchedOut *out) {
    int i;

    printf("algo=%s order=", algo);
    for (i = 0; i < out->n; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%d", out->track[i]);
    }
    printf(" move=%d\n", out->move);
}

static int take_at(int *q, int n, int idx) {
    int track = q[idx];
    int i;

    for (i = idx; i < n - 1; i++) {
        q[i] = q[i + 1];
    }
    return track;
}

static int closer(int track, int best, int head, int ahead_only) {
    int dist = iabs(track - head);
    int best_dist = iabs(best - head);

    if (ahead_only && track < head && best >= head) {
        return 0;
    }
    if (ahead_only && track >= head && best < head) {
        return 1;
    }
    if (dist != best_dist) {
        return dist < best_dist;
    }
    return track < best;
}

/* 这组数据方向固定向上。SCAN / C-SCAN 先在未越过的磁道里挑，没有了才回头。 */
static int stick_pick(const char *algo, const int *q, int n, int head) {
    int best = 0;
    int ahead = strcmp(algo, "scan") == 0 || strcmp(algo, "cscan") == 0;
    int i;

    if (strcmp(algo, "fcfs") == 0) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        if (closer(q[i], q[best], head, ahead)) {
            best = i;
        }
    }
    return best;
}

int stick_run(const char *algo, int extra, int *order, int *n, int *stuck) {
    int q[16];
    int qn = 2;
    int head = 50;
    int injected = 0;
    int left = 0;

    if (extra < 0 || extra > 8) {
        return -1;
    }
    if (strcmp(algo, "fcfs") != 0 && strcmp(algo, "sstf") != 0 && strcmp(algo, "scan") != 0 &&
        strcmp(algo, "cscan") != 0) {
        return -1;
    }
    q[0] = 50;
    q[1] = 80;
    *n = 0;
    while (qn > 0 && *n < 16) {
        int idx = stick_pick(algo, q, qn, head);
        int track = take_at(q, qn, idx);

        qn--;
        order[(*n)++] = track;
        if (track != 50 && injected < extra) {
            left = 1;
        }
        if (track == 50 && injected < extra) {
            q[qn++] = 50;
            injected++;
        }
        head = track;
    }
    *stuck = extra > 0 && !left;
    return 0;
}
