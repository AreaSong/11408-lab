#include "dev/dev_alloc_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAY(s, ...)                                    \
    do {                                               \
        if ((s)->out != NULL) {                        \
            fprintf((s)->out, __VA_ARGS__);            \
        }                                              \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0' && i < DEV_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int find_named(const char *name, int n, size_t stride, const char *base) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(base + (size_t)i * stride, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_dev(const DevSys *s, const char *name) {
    return find_named(name, s->ndev, sizeof(DevNode), s->dev[0].name);
}

static int find_ctrl(const DevSys *s, const char *name) {
    return find_named(name, s->nctrl, sizeof(DevCtrl), s->ctrl[0].name);
}

static int find_chan(const DevSys *s, const char *name) {
    return find_named(name, s->nchan, sizeof(DevChan), s->chan[0].name);
}

static int find_proc(const DevSys *s, const char *name) {
    return find_named(name, s->nproc, sizeof(DevProc), s->proc[0].name);
}

static int find_map(const DevProc *p, const char *logic) {
    int i;
    for (i = 0; i < p->nmap; i++) {
        if (strcmp(p->map[i].logic, logic) == 0) {
            return i;
        }
    }
    return -1;
}

const char *dev_stage_text(int stage) {
    switch (stage) {
    case ST_WAIT_DEV: return "wait-dev";
    case ST_WAIT_CTRL: return "wait-ctrl";
    case ST_WAIT_CHAN: return "wait-chan";
    case ST_IO: return "io";
    default: return "free";
    }
}

const char *dev_owner_name(const DevSys *s, int pi) {
    return pi < 0 ? "-" : s->proc[pi].name;
}

static void q_pop(DevQ *q) {
    int i;
    for (i = 1; i < q->n; i++) {
        q->id[i - 1] = q->id[i];
    }
    q->n--;
}

static void block_on(DevSys *s, int pi, int stage, DevQ *q, const char *level, const char *res) {
    if (q->n >= DEV_Q) {
        SAY(s, "result=full\n");
        s->proc[pi].stage = ST_FREE;
        return;
    }
    q->id[q->n++] = pi;
    s->proc[pi].stage = stage;
    SAY(s, "block p=%s level=%s queue=%s\n", s->proc[pi].name, level, res);
}

static void hold_chan(DevSys *s, int pi);
static void hold_ctrl(DevSys *s, int pi);

static void hold_dev(DevSys *s, int pi) {
    int di = s->proc[pi].dev;
    if (s->dev[di].owner >= 0) {
        block_on(s, pi, ST_WAIT_DEV, &s->dev[di].q, "device", s->dev[di].name);
        return;
    }
    s->dev[di].owner = pi;
    SAY(s, "grant p=%s device=%s\n", s->proc[pi].name, s->dev[di].name);
    hold_ctrl(s, pi);
}

static void hold_ctrl(DevSys *s, int pi) {
    int ci = s->proc[pi].ctrl;
    if (s->ctrl[ci].owner >= 0) {
        block_on(s, pi, ST_WAIT_CTRL, &s->ctrl[ci].q, "controller", s->ctrl[ci].name);
        return;
    }
    s->ctrl[ci].owner = pi;
    SAY(s, "grant p=%s controller=%s\n", s->proc[pi].name, s->ctrl[ci].name);
    hold_chan(s, pi);
}

static void hold_chan(DevSys *s, int pi) {
    int hi = s->proc[pi].chan;
    if (s->chan[hi].owner >= 0) {
        block_on(s, pi, ST_WAIT_CHAN, &s->chan[hi].q, "channel", s->chan[hi].name);
        return;
    }
    s->chan[hi].owner = pi;
    s->proc[pi].stage = ST_IO;
    SAY(s, "grant p=%s channel=%s\n", s->proc[pi].name, s->chan[hi].name);
    SAY(s, "state p=%s stage=io\n", s->proc[pi].name);
}

static void serve_chan(DevSys *s, int hi) {
    int pi;
    if (s->chan[hi].q.n == 0) {
        return;
    }
    pi = s->chan[hi].q.id[0];
    q_pop(&s->chan[hi].q);
    hold_chan(s, pi);
}

static void serve_ctrl(DevSys *s, int ci) {
    int pi;
    if (s->ctrl[ci].q.n == 0) {
        return;
    }
    pi = s->ctrl[ci].q.id[0];
    q_pop(&s->ctrl[ci].q);
    hold_ctrl(s, pi);
}

static void serve_dev(DevSys *s, int di) {
    int pi;
    if (s->dev[di].q.n == 0) {
        return;
    }
    pi = s->dev[di].q.id[0];
    q_pop(&s->dev[di].q);
    hold_dev(s, pi);
}

DevSys *dev_sys_new(FILE *out) {
    DevSys *s = calloc(1, sizeof *s);
    int i;
    if (s == NULL) {
        return NULL;
    }
    s->out = out;
    for (i = 0; i < DEV_N; i++) {
        s->dev[i].owner = -1;
        s->dev[i].ctrl = -1;
    }
    for (i = 0; i < DEV_CTRL; i++) {
        s->ctrl[i].owner = -1;
        s->ctrl[i].chan = -1;
    }
    for (i = 0; i < DEV_CHAN; i++) {
        s->chan[i].owner = -1;
    }
    for (i = 0; i < DEV_PROC; i++) {
        s->proc[i].dev = -1;
        s->proc[i].ctrl = -1;
        s->proc[i].chan = -1;
    }
    return s;
}

void dev_sys_free(DevSys *s) { free(s); }

static int ensure_chan(DevSys *s, const char *name) {
    int hi = find_chan(s, name);
    if (hi >= 0) {
        return hi;
    }
    if (s->nchan >= DEV_CHAN) {
        return -2;
    }
    hi = s->nchan++;
    ncopy(s->chan[hi].name, name);
    s->chan[hi].owner = -1;
    return hi;
}

static int ensure_ctrl(DevSys *s, const char *name, int hi) {
    int ci = find_ctrl(s, name);
    if (ci >= 0) {
        return s->ctrl[ci].chan == hi ? ci : -3;
    }
    if (s->nctrl >= DEV_CTRL) {
        return -2;
    }
    ci = s->nctrl++;
    ncopy(s->ctrl[ci].name, name);
    s->ctrl[ci].chan = hi;
    s->ctrl[ci].owner = -1;
    return ci;
}

int dev_sys_add(DevSys *s, const char *type, const char *dev, const char *ctrl, const char *chan) {
    int hi;
    int ci;
    int di;
    if (s == NULL || type == NULL || dev == NULL || ctrl == NULL || chan == NULL) {
        return -1;
    }
    if (strcmp(type, "block") != 0 && strcmp(type, "char") != 0) {
        return -1;
    }
    if (find_dev(s, dev) >= 0) {
        SAY(s, "result=dup\n");
        return 0;
    }
    if (s->ndev >= DEV_N) {
        SAY(s, "result=full\n");
        return 0;
    }
    ci = find_ctrl(s, ctrl);
    hi = find_chan(s, chan);
    if (ci >= 0 && (hi < 0 || s->ctrl[ci].chan != hi)) {
        SAY(s, "result=mismatch\n");
        return 0;
    }
    if ((hi < 0 && s->nchan >= DEV_CHAN) || (ci < 0 && s->nctrl >= DEV_CTRL)) {
        SAY(s, "result=full\n");
        return 0;
    }
    hi = ensure_chan(s, chan);
    ci = ensure_ctrl(s, ctrl, hi);
    if (hi < 0 || ci < 0) {
        SAY(s, "result=full\n");
        return 0;
    }
    di = s->ndev++;
    ncopy(s->dev[di].name, dev);
    ncopy(s->dev[di].type, type);
    s->dev[di].ctrl = ci;
    s->dev[di].owner = -1;
    SAY(s, "add device=%s type=%s ctrl=%s chan=%s\n", dev, type, ctrl, chan);
    return 0;
}

int dev_sys_proc(DevSys *s, const char *name) {
    int pi;
    if (s == NULL || name == NULL) {
        return -1;
    }
    if (find_proc(s, name) >= 0) {
        SAY(s, "result=dup\n");
        return 0;
    }
    if (s->nproc >= DEV_PROC) {
        SAY(s, "result=full\n");
        return 0;
    }
    pi = s->nproc++;
    ncopy(s->proc[pi].name, name);
    s->proc[pi].dev = -1;
    s->proc[pi].ctrl = -1;
    s->proc[pi].chan = -1;
    SAY(s, "proc name=%s\n", name);
    return 0;
}

int dev_sys_map(DevSys *s, const char *proc, const char *logic, const char *phys) {
    int pi;
    int di;
    int mi;
    DevProc *p;
    if (s == NULL || proc == NULL || logic == NULL || phys == NULL) {
        return -1;
    }
    pi = find_proc(s, proc);
    di = find_dev(s, phys);
    if (pi < 0 || di < 0) {
        SAY(s, "result=none\n");
        return 0;
    }
    p = &s->proc[pi];
    mi = find_map(p, logic);
    if (mi < 0) {
        if (p->nmap >= DEV_MAP) {
            SAY(s, "result=full\n");
            return 0;
        }
        mi = p->nmap++;
        ncopy(p->map[mi].logic, logic);
    }
    p->map[mi].dev = di;
    SAY(s, "map p=%s logic=%s phys=%s\n", proc, logic, phys);
    return 0;
}

int dev_sys_ask(DevSys *s, const char *proc, const char *logic) {
    int pi;
    int mi;
    int di;
    DevProc *p;
    if (s == NULL || proc == NULL || logic == NULL) {
        return -1;
    }
    pi = find_proc(s, proc);
    if (pi < 0) {
        SAY(s, "result=none\n");
        return 0;
    }
    p = &s->proc[pi];
    if (p->stage != ST_FREE) {
        SAY(s, "result=busy-proc\n");
        return 0;
    }
    mi = find_map(p, logic);
    if (mi < 0) {
        SAY(s, "result=no-map\n");
        return 0;
    }
    di = p->map[mi].dev;
    p->dev = di;
    p->ctrl = s->dev[di].ctrl;
    p->chan = s->ctrl[p->ctrl].chan;
    SAY(s, "ask p=%s logic=%s phys=%s\n", proc, logic, s->dev[di].name);
    hold_dev(s, pi);
    return 0;
}

int dev_sys_done(DevSys *s, const char *proc) {
    int pi;
    int di;
    int ci;
    int hi;
    DevProc *p;
    if (s == NULL || proc == NULL) {
        return -1;
    }
    pi = find_proc(s, proc);
    if (pi < 0) {
        SAY(s, "result=none\n");
        return 0;
    }
    p = &s->proc[pi];
    if (p->stage != ST_IO) {
        SAY(s, "result=not-io\n");
        return 0;
    }
    di = p->dev;
    ci = p->ctrl;
    hi = p->chan;
    p->stage = ST_FREE;
    p->dev = p->ctrl = p->chan = -1;
    s->chan[hi].owner = -1;
    s->ctrl[ci].owner = -1;
    s->dev[di].owner = -1;
    SAY(s, "done p=%s\n", proc);
    SAY(s, "free p=%s channel=%s\n", proc, s->chan[hi].name);
    SAY(s, "free p=%s controller=%s\n", proc, s->ctrl[ci].name);
    SAY(s, "free p=%s device=%s\n", proc, s->dev[di].name);
    serve_chan(s, hi);
    serve_ctrl(s, ci);
    serve_dev(s, di);
    return 0;
}

const char *dev_sys_stage(const DevSys *s, const char *proc) {
    int pi;
    if (s == NULL || proc == NULL) {
        return NULL;
    }
    pi = find_proc(s, proc);
    if (pi < 0) {
        return NULL;
    }
    return dev_stage_text(s->proc[pi].stage);
}

int dev_sys_counts(const DevSys *s, int *ndev, int *nctrl, int *nchan, int *nproc) {
    if (s == NULL) {
        return -1;
    }
    if (ndev) {
        *ndev = s->ndev;
    }
    if (nctrl) {
        *nctrl = s->nctrl;
    }
    if (nchan) {
        *nchan = s->nchan;
    }
    if (nproc) {
        *nproc = s->nproc;
    }
    return 0;
}
