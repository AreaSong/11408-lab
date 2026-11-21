#include "dev/dev_alloc_i.h"

#include <stdio.h>

static void show_q(const DevSys *s, const DevQ *q) {
    int i;
    if (q->n == 0) {
        fprintf(s->out, "-");
        return;
    }
    for (i = 0; i < q->n; i++) {
        if (i > 0) {
            fprintf(s->out, ",");
        }
        fprintf(s->out, "%s", s->proc[q->id[i]].name);
    }
}

static const char *dev_name(const DevSys *s, int di) {
    return di < 0 ? "-" : s->dev[di].name;
}

static const char *ctrl_name(const DevSys *s, int ci) {
    return ci < 0 ? "-" : s->ctrl[ci].name;
}

static const char *chan_name(const DevSys *s, int hi) {
    return hi < 0 ? "-" : s->chan[hi].name;
}

static void show_tables(const DevSys *s) {
    int i;
    for (i = 0; i < s->ndev; i++) {
        const DevNode *d = &s->dev[i];
        const char *st = d->owner < 0 ? "free" : dev_stage_text(s->proc[d->owner].stage);
        fprintf(s->out, "dct name=%s type=%s ctrl=%s owner=%s stage=%s wait=", d->name, d->type,
                ctrl_name(s, d->ctrl), dev_owner_name(s, d->owner), st);
        show_q(s, &d->q);
        fprintf(s->out, "\n");
    }
    for (i = 0; i < s->nctrl; i++) {
        const DevCtrl *c = &s->ctrl[i];
        const char *st = c->owner < 0 ? "free" : dev_stage_text(s->proc[c->owner].stage);
        fprintf(s->out, "coct name=%s chan=%s owner=%s stage=%s wait=", c->name, chan_name(s, c->chan),
                dev_owner_name(s, c->owner), st);
        show_q(s, &c->q);
        fprintf(s->out, "\n");
    }
    for (i = 0; i < s->nchan; i++) {
        const DevChan *h = &s->chan[i];
        const char *st = h->owner < 0 ? "free" : dev_stage_text(s->proc[h->owner].stage);
        fprintf(s->out, "chct name=%s owner=%s stage=%s wait=", h->name, dev_owner_name(s, h->owner), st);
        show_q(s, &h->q);
        fprintf(s->out, "\n");
    }
}

static void show_procs(const DevSys *s) {
    int i;
    int k;
    for (i = 0; i < s->nproc; i++) {
        const DevProc *p = &s->proc[i];
        fprintf(s->out, "proc name=%s stage=%s device=%s ctrl=%s chan=%s\n", p->name, dev_stage_text(p->stage),
                dev_name(s, p->dev), ctrl_name(s, p->ctrl), chan_name(s, p->chan));
        for (k = 0; k < p->nmap; k++) {
            fprintf(s->out, "lut p=%s logic=%s phys=%s\n", p->name, p->map[k].logic, dev_name(s, p->map[k].dev));
        }
    }
}

void dev_sys_show(const DevSys *s) {
    if (s == NULL || s->out == NULL) {
        return;
    }
    fprintf(s->out, "sdt devices=%d controllers=%d channels=%d procs=%d\n", s->ndev, s->nctrl, s->nchan,
            s->nproc);
    show_tables(s);
    show_procs(s);
}
