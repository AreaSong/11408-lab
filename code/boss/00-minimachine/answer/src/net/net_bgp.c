#include "net/net_bgp.h"

#include <stdio.h>
#include <string.h>

int net_bgp_drop(const int *path, int n, int self) {
    int i;
    if (path == NULL || n < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (path[i] == self) {
            return 1;
        }
    }
    return 0;
}

int net_bgp_kind(int as_a, int as_b, char *out, int cap) {
    int n;
    if (out == NULL || as_a <= 0 || as_b <= 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s", as_a == as_b ? "iBGP" : "eBGP");
    return n > 0 && n < cap ? 0 : -1;
}

static const char *pick(const int *as_count, const int *igp, const char *const *name, int n) {
    int best = 0;
    int i;
    for (i = 1; i < n; i++) {
        if (as_count[i] < as_count[best] || (as_count[i] == as_count[best] && igp[i] < igp[best])) {
            best = i;
        }
    }
    return name[best];
}

int net_bgp_hop(int router, char *out, int cap) {
    static const char *name[] = {"R11", "R12", "R13"};
    static const int as_count[] = {3, 4, 3};
    static const int igp14[] = {1, 2, 2};
    static const int igp15[] = {2, 2, 1};
    const char *hop;
    int n;
    if (router == 14) {
        hop = pick(as_count, igp14, name, 3);
    } else if (router == 15) {
        hop = pick(as_count, igp15, name, 3);
    } else {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s", hop);
    return n > 0 && n < cap ? 0 : -1;
}

int net_bgp_line(const char *id, char *out, int cap) {
    char a[8];
    char b[8];
    int path[] = {2, 1};
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2017-37") == 0) {
        n = snprintf(out, (size_t)cap, "UDP、IP、TCP");
        return n > 0 && n < cap ? 0 : -1;
    }
    if (strcmp(id, "loop") == 0) {
        n = snprintf(out, (size_t)cap, "drop=%d", net_bgp_drop(path, 2, 1));
        return n > 0 && n < cap ? 0 : -1;
    }
    if (strcmp(id, "2024-47") != 0 || net_bgp_kind(4, 1, a, (int)sizeof a) != 0 ||
        net_bgp_hop(14, b, (int)sizeof b) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%s UPDATE iBGP R14=%s", a, b);
    if (n < 0 || n >= cap || net_bgp_hop(15, b, (int)sizeof b) != 0) {
        return -1;
    }
    n += snprintf(out + n, (size_t)cap - (size_t)n, " R15=%s", b);
    return n > 0 && n < cap ? 0 : -1;
}

int net_nexam(const char *id, char *letter, int cap) {
    static const struct {
        const char *id;
        const char *letter;
    } row[] = {
        {"2010-37", "B"}, {"2011-37", "D"}, {"2011-38", "C"}, {"2012-39", "D"}, {"2016-38", "D"},
        {"2017-37", "D"}, {"2018-38", "C"}, {"2019-37", "B"}, {"2021-35", "B"}, {"2021-36", "B"},
        {"2022-37", "B"}, {"2023-40", "D"}, {"2024-35", "D"},
    };
    int i;
    if (id == NULL || letter == NULL) {
        return -1;
    }
    for (i = 0; i < (int)(sizeof row / sizeof row[0]); i++) {
        if (strcmp(row[i].id, id) == 0) {
            snprintf(letter, (size_t)cap, "%s", row[i].letter);
            return 0;
        }
    }
    return -1;
}
