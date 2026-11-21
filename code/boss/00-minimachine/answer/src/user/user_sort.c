#include "user/user_sort.h"

#include "balance.h"
#include "fs/fs_i.h"
#include "loser.h"
#include "repl.h"

#include <stdio.h>
#include <string.h>

static int pick_min(const int *a, const int *dead, int m) {
    int i;
    int best = -1;
    for (i = 0; i < m; i++) {
        if (dead[i]) {
            continue;
        }
        if (best < 0 || a[i] < a[best]) {
            best = i;
        }
    }
    return best;
}

int user_sort_wpl(const int *len, int n, int ways, long long *wpl) {
    int a[32];
    int dead[32];
    int dummy;
    int m = 0;
    int i;
    long long sum = 0;
    if (len == NULL || wpl == NULL || n < 1 || n > 16 || ways < 2 || ways > 8) {
        return -1;
    }
    dummy = dummy_count(n, ways);
    if (dummy < 0 || n + dummy > 32) {
        return -1;
    }
    for (i = 0; i < dummy; i++) {
        a[m++] = 0;
    }
    for (i = 0; i < n; i++) {
        a[m++] = len[i];
    }
    while (m > 1) {
        int take = ways < m ? ways : m;
        int got = 0;
        int part = 0;
        int w = 0;
        memset(dead, 0, sizeof dead);
        while (got < take) {
            int at = pick_min(a, dead, m);
            if (at < 0) {
                return -1;
            }
            dead[at] = 1;
            part += a[at];
            got++;
        }
        sum += part;
        for (i = 0; i < m; i++) {
            if (!dead[i]) {
                a[w++] = a[i];
            }
        }
        a[w++] = part;
        m = w;
    }
    *wpl = sum;
    return 0;
}

static int line_num(char *out, int cap, const char *text) {
    int n = snprintf(out, (size_t)cap, "%s", text);
    return n > 0 && n < cap ? 0 : -1;
}

static int line_best(const int *len, int n, int ways, int with_cmp, char *out, int cap) {
    long long wpl = 0;
    int wrote;
    if (user_sort_wpl(len, n, ways, &wpl) != 0) {
        return -1;
    }
    if (with_cmp) {
        wrote = snprintf(out, (size_t)cap, "cmp=%lld io=%lld", wpl - (n - 1), wpl * 2);
    } else {
        wrote = snprintf(out, (size_t)cap, "wpl=%lld io=%lld", wpl, wpl * 2);
    }
    return wrote > 0 && wrote < cap ? 0 : -1;
}

int user_sort_line(const char *id, char *out, int cap) {
    static const int nine[] = {9, 30, 12, 18, 3, 17, 2, 6, 24};
    static const int six[] = {10, 35, 40, 50, 60, 200};
    int passes;
    long long io;
    LoseIn in;
    LoseResult got;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2019-11") == 0) {
        return line_num(out, cap, "dummy=2 letter=B") == 0 && dummy_count(120, 12) == 2 ? 0 : -1;
    }
    if (strcmp(id, "2012-41") == 0) {
        return line_best(six, 6, 2, 1, out, cap);
    }
    if (strcmp(id, "practice-best") == 0) {
        return line_best(nine, 9, 3, 0, out, cap);
    }
    if (strcmp(id, "practice-io") == 0) {
        passes = merge_passes(625, 5);
        io = balance_io(375000, passes);
        if (passes != 4 || io != 3000000) {
            return -1;
        }
        return line_num(out, cap, "passes=4 io=3000000");
    }
    if (strcmp(id, "2016-11") == 0) {
        return line_num(out, cap, "algo=merge letter=D");
    }
    if (strcmp(id, "2024-11") != 0) {
        return -1;
    }
    memset(&in, 0, sizeof in);
    in.k = 3;
    in.len[0] = in.len[1] = in.len[2] = 1;
    in.data[0][0] = 10;
    in.data[1][0] = 3;
    in.data[2][0] = 7;
    if (loser_merge(&in, &got) != 0) {
        return -1;
    }
    passes = snprintf(out, (size_t)cap, "seg=%d key=%d letter=D", got.winner_seg, got.winner_key);
    return passes > 0 && passes < cap ? 0 : -1;
}

static int parse_ints(const char *text, int *in, int cap, int *n) {
    const char *p = text == NULL ? "" : text;
    *n = 0;
    while (*p != '\0') {
        int value;
        int used = 0;
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (sscanf(p, "%d%n", &value, &used) != 1 || used <= 0) {
            return -1;
        }
        if (*n >= cap) {
            return -1;
        }
        in[(*n)++] = value;
        p += used;
    }
    return *n > 0 ? 0 : -1;
}

static int take_sorted(const Repl *r, int *sorted) {
    LoseIn in;
    LoseResult got;
    int i;
    int at = 0;
    if (r->runs < 2) {
        for (i = 0; i < r->n; i++) {
            sorted[i] = r->key[i];
        }
        return 0;
    }
    if (r->runs > LOSER_K) {
        return -1;
    }
    memset(&in, 0, sizeof in);
    in.k = r->runs;
    for (i = 0; i < r->runs; i++) {
        int k;
        if (r->len[i] > LOSER_SEG || r->len[i] < 1) {
            return -1;
        }
        in.len[i] = r->len[i];
        for (k = 0; k < r->len[i]; k++) {
            in.data[i][k] = r->key[at++];
        }
    }
    if (loser_merge(&in, &got) != 0 || got.n != r->n) {
        return -1;
    }
    for (i = 0; i < got.n; i++) {
        sorted[i] = got.out[i];
    }
    return 0;
}

static int store_runs(FsNs *ns, const Repl *r) {
    FsOut out;
    int i;
    int at = 0;
    if (ns == NULL) {
        return 0;
    }
    if (fs_ns_stat(ns, "/s", 0, &out) != FS_OK && fs_ns_mkdir(ns, "/s", &out) != FS_OK) {
        return -1;
    }
    for (i = 0; i < r->runs; i++) {
        char path[16];
        char body[FS_TEXT];
        int k;
        int n = 0;
        snprintf(path, sizeof path, "/s/r%d", i);
        body[0] = '\0';
        for (k = 0; k < r->len[i]; k++) {
            int w = snprintf(body + n, sizeof body - (size_t)n, "%s%d", n ? " " : "", r->key[at++]);
            if (w < 0 || n + w >= (int)sizeof body) {
                return -1;
            }
            n += w;
        }
        fs_ns_unlink(ns, path, &out);
        if (fs_ns_create(ns, path, body, &out) != FS_OK) {
            return -1;
        }
    }
    return 0;
}

static int join_nums(const int *v, int n, char *out, int cap) {
    int i;
    int at = 0;
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int w = snprintf(out + at, (size_t)(cap - at), "%s%d", i ? " " : "", v[i]);
        if (w < 0 || at + w >= cap) {
            return -1;
        }
        at += w;
    }
    return 0;
}

int user_sort_text(FsNs *ns, int memory, const char *text, char *out, int cap) {
    int raw[REPL_N];
    int sorted[REPL_N];
    int n = 0;
    int passes;
    long long io;
    char nums[160];
    int wrote;
    Repl repl;
    if (out == NULL || parse_ints(text, raw, REPL_N, &n) != 0 || repl_select(memory, raw, n, &repl) != 0) {
        return -1;
    }
    passes = merge_passes(repl.runs, 3);
    io = balance_io(n, passes);
    if (passes < 0 || io < 0 || take_sorted(&repl, sorted) != 0 || store_runs(ns, &repl) != 0) {
        return -1;
    }
    if (join_nums(sorted, n, nums, (int)sizeof nums) != 0) {
        return -1;
    }
    wrote = snprintf(out, (size_t)cap, "%s\nhost=sort runs=%d first=%d passes=%d gen=%d io=%lld files=%d", nums,
                     repl.runs, repl.first, passes, n * 2, io, ns == NULL ? 0 : repl.runs);
    return wrote > 0 && wrote < cap ? 0 : -1;
}
