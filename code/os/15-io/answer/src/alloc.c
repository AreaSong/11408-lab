#include "alloc.h"

#include <stdio.h>
#include <string.h>

#define SAY(s, ...)              \
    do {                         \
        if (!(s)->quiet) {       \
            printf(__VA_ARGS__); \
        }                        \
    } while (0)

static void ncopy(char *dst, const char *src) {
    int i = 0;

    while (src[i] != '\0' && i < IO_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int find_dev(const IoSys *s, const char *name) {
    int i;

    for (i = 0; i < s->ndev; i++) {
        if (strcmp(s->dev[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_ctrl(const IoSys *s, const char *name) {
    int i;

    for (i = 0; i < s->nctrl; i++) {
        if (strcmp(s->ctrl[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_chan(const IoSys *s, const char *name) {
    int i;

    for (i = 0; i < s->nchan; i++) {
        if (strcmp(s->chan[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_proc(const IoSys *s, const char *name) {
    int i;

    for (i = 0; i < s->nproc; i++) {
        if (strcmp(s->proc[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_map(const IoProc *p, const char *logic) {
    int i;

    for (i = 0; i < p->nmap; i++) {
        if (strcmp(p->map[i].logic, logic) == 0) {
            return i;
        }
    }
    return -1;
}

static const char *stage_text(int stage) {
    switch (stage) {
    case ST_WAIT_DEV:
        return "wait-dev";
    case ST_WAIT_CTRL:
        return "wait-ctrl";
    case ST_WAIT_CHAN:
        return "wait-chan";
    case ST_IO:
        return "io";
    default:
        return "free";
    }
}

static const char *who(const IoSys *s, int pi) {
    return pi < 0 ? "-" : s->proc[pi].name;
}

static void q_pop(IoQ *q) {
    int i;

    for (i = 1; i < q->n; i++) {
        q->id[i - 1] = q->id[i];
    }
    q->n--;
}

static void block_on(IoSys *s, int pi, int stage, IoQ *q, const char *level, const char *res) {
    if (q->n >= IO_Q) {
        SAY(s, "result=full\n");
        s->proc[pi].stage = ST_FREE;
        return;
    }
    q->id[q->n++] = pi;
    s->proc[pi].stage = stage;
    SAY(s, "block p=%s level=%s queue=%s\n", s->proc[pi].name, level, res);
}

static void hold_chan(IoSys *s, int pi);
static void hold_ctrl(IoSys *s, int pi);

static void hold_dev(IoSys *s, int pi) {
    int di = s->proc[pi].dev;

    if (s->dev[di].owner >= 0) {
        block_on(s, pi, ST_WAIT_DEV, &s->dev[di].q, "device", s->dev[di].name);
        return;
    }
    s->dev[di].owner = pi;
    SAY(s, "grant p=%s device=%s\n", s->proc[pi].name, s->dev[di].name);
    hold_ctrl(s, pi);
}

static void hold_ctrl(IoSys *s, int pi) {
    int ci = s->proc[pi].ctrl;

    if (s->ctrl[ci].owner >= 0) {
        block_on(s, pi, ST_WAIT_CTRL, &s->ctrl[ci].q, "controller", s->ctrl[ci].name);
        return;
    }
    s->ctrl[ci].owner = pi;
    SAY(s, "grant p=%s controller=%s\n", s->proc[pi].name, s->ctrl[ci].name);
    hold_chan(s, pi);
}

static void hold_chan(IoSys *s, int pi) {
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

static void serve_chan(IoSys *s, int hi) {
    int pi;

    if (s->chan[hi].q.n == 0) {
        return;
    }
    pi = s->chan[hi].q.id[0];
    q_pop(&s->chan[hi].q);
    hold_chan(s, pi);
}

static void serve_ctrl(IoSys *s, int ci) {
    int pi;

    if (s->ctrl[ci].q.n == 0) {
        return;
    }
    pi = s->ctrl[ci].q.id[0];
    q_pop(&s->ctrl[ci].q);
    hold_ctrl(s, pi);
}

static void serve_dev(IoSys *s, int di) {
    int pi;

    if (s->dev[di].q.n == 0) {
        return;
    }
    pi = s->dev[di].q.id[0];
    q_pop(&s->dev[di].q);
    hold_dev(s, pi);
}

void io_init(IoSys *s) {
    int quiet = s->quiet;
    int i;

    memset(s, 0, sizeof *s);
    s->quiet = quiet;
    for (i = 0; i < IO_DEV; i++) {
        s->dev[i].owner = -1;
        s->dev[i].ctrl = -1;
    }
    for (i = 0; i < IO_CTRL; i++) {
        s->ctrl[i].owner = -1;
        s->ctrl[i].chan = -1;
    }
    for (i = 0; i < IO_CHAN; i++) {
        s->chan[i].owner = -1;
    }
    for (i = 0; i < IO_PROC; i++) {
        s->proc[i].dev = -1;
        s->proc[i].ctrl = -1;
        s->proc[i].chan = -1;
    }
}

static int ensure_chan(IoSys *s, const char *name) {
    int hi = find_chan(s, name);

    if (hi >= 0) {
        return hi;
    }
    if (s->nchan >= IO_CHAN) {
        return -2;
    }
    hi = s->nchan++;
    ncopy(s->chan[hi].name, name);
    s->chan[hi].owner = -1;
    return hi;
}

static int ensure_ctrl(IoSys *s, const char *name, int hi) {
    int ci = find_ctrl(s, name);

    if (ci >= 0) {
        return s->ctrl[ci].chan == hi ? ci : -3;
    }
    if (s->nctrl >= IO_CTRL) {
        return -2;
    }
    ci = s->nctrl++;
    ncopy(s->ctrl[ci].name, name);
    s->ctrl[ci].chan = hi;
    s->ctrl[ci].owner = -1;
    return ci;
}

static int add_ready(IoSys *s, const char *ctrl, const char *chan) {
    int ci = find_ctrl(s, ctrl);
    int hi = find_chan(s, chan);

    if (ci >= 0) {
        return (hi >= 0 && s->ctrl[ci].chan == hi) ? 0 : -3;
    }
    if (s->nctrl >= IO_CTRL) {
        return -2;
    }
    if (hi < 0 && s->nchan >= IO_CHAN) {
        return -2;
    }
    return 0;
}

int io_add(IoSys *s, const char *type, const char *dev, const char *ctrl, const char *chan) {
    int hi;
    int ci;
    int di;
    int ready;

    if (find_dev(s, dev) >= 0) {
        SAY(s, "result=dup\n");
        return 0;
    }
    if (s->ndev >= IO_DEV) {
        SAY(s, "result=full\n");
        return 0;
    }
    ready = add_ready(s, ctrl, chan);
    if (ready == -2) {
        SAY(s, "result=full\n");
        return 0;
    }
    if (ready == -3) {
        SAY(s, "result=mismatch\n");
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

int io_proc(IoSys *s, const char *name) {
    int pi;

    if (find_proc(s, name) >= 0) {
        SAY(s, "result=dup\n");
        return 0;
    }
    if (s->nproc >= IO_PROC) {
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

int io_map(IoSys *s, const char *proc, const char *logic, const char *phys) {
    int pi = find_proc(s, proc);
    int di = find_dev(s, phys);
    int mi;
    IoProc *p;

    if (pi < 0 || di < 0) {
        SAY(s, "result=none\n");
        return 0;
    }
    p = &s->proc[pi];
    mi = find_map(p, logic);
    if (mi < 0) {
        if (p->nmap >= IO_MAP) {
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

int io_ask(IoSys *s, const char *proc, const char *logic) {
    int pi = find_proc(s, proc);
    int mi;
    int di;
    IoProc *p;

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

int io_done(IoSys *s, const char *proc) {
    int pi = find_proc(s, proc);
    int di;
    int ci;
    int hi;
    IoProc *p;

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
    p->dev = -1;
    p->ctrl = -1;
    p->chan = -1;
    s->chan[hi].owner = -1;
    s->ctrl[ci].owner = -1;
    s->dev[di].owner = -1;
    /* 先还通道，再还控制器，再还设备。已经占着上游的进程先继续，避免新进程抢走通道。 */
    SAY(s, "done p=%s\n", proc);
    SAY(s, "free p=%s channel=%s\n", proc, s->chan[hi].name);
    SAY(s, "free p=%s controller=%s\n", proc, s->ctrl[ci].name);
    SAY(s, "free p=%s device=%s\n", proc, s->dev[di].name);
    serve_chan(s, hi);
    serve_ctrl(s, ci);
    serve_dev(s, di);
    return 0;
}

static void show_q(const IoSys *s, const IoQ *q) {
    int i;

    if (q->n == 0) {
        printf("-");
        return;
    }
    for (i = 0; i < q->n; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s", s->proc[q->id[i]].name);
    }
}

static const char *dev_name(const IoSys *s, int di) {
    return di < 0 ? "-" : s->dev[di].name;
}

static const char *ctrl_name(const IoSys *s, int ci) {
    return ci < 0 ? "-" : s->ctrl[ci].name;
}

static const char *chan_name(const IoSys *s, int hi) {
    return hi < 0 ? "-" : s->chan[hi].name;
}

static void show_tables(const IoSys *s) {
    int i;

    for (i = 0; i < s->ndev; i++) {
        const IoDev *d = &s->dev[i];
        const char *st = d->owner < 0 ? "free" : stage_text(s->proc[d->owner].stage);
        printf("dct name=%s type=%s ctrl=%s owner=%s stage=%s wait=", d->name, d->type,
               ctrl_name(s, d->ctrl), who(s, d->owner), st);
        show_q(s, &d->q);
        printf("\n");
    }
    for (i = 0; i < s->nctrl; i++) {
        const IoCtrl *c = &s->ctrl[i];
        const char *st = c->owner < 0 ? "free" : stage_text(s->proc[c->owner].stage);
        printf("coct name=%s chan=%s owner=%s stage=%s wait=", c->name, chan_name(s, c->chan),
               who(s, c->owner), st);
        show_q(s, &c->q);
        printf("\n");
    }
    for (i = 0; i < s->nchan; i++) {
        const IoChan *h = &s->chan[i];
        const char *st = h->owner < 0 ? "free" : stage_text(s->proc[h->owner].stage);
        printf("chct name=%s owner=%s stage=%s wait=", h->name, who(s, h->owner), st);
        show_q(s, &h->q);
        printf("\n");
    }
}

static void show_procs(const IoSys *s) {
    int i;
    int k;

    for (i = 0; i < s->nproc; i++) {
        const IoProc *p = &s->proc[i];
        printf("proc name=%s stage=%s device=%s ctrl=%s chan=%s\n", p->name, stage_text(p->stage),
               dev_name(s, p->dev), ctrl_name(s, p->ctrl), chan_name(s, p->chan));
        for (k = 0; k < p->nmap; k++) {
            printf("lut p=%s logic=%s phys=%s\n", p->name, p->map[k].logic,
                   dev_name(s, p->map[k].dev));
        }
    }
}

void io_show(const IoSys *s) {
    printf("sdt devices=%d controllers=%d channels=%d procs=%d\n", s->ndev, s->nctrl, s->nchan,
           s->nproc);
    show_tables(s);
    show_procs(s);
}
