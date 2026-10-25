#include "rw.h"
#include "rw_state.h"

#include <stdio.h>
#include <string.h>

int scene_writer_cut(Scene *out);

static int reader_in(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 0:
            if (pv_wait(w, who, r->rmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 1;
            return PV_YIELD;
        case 1:
            r->readcount++;
            if (r->readcount > r->max_read) {
                r->max_read = r->readcount;
            }
            pv_count(w, who, "readcount", r->readcount);
            *phase = 2;
            return PV_YIELD;
        case 2:
            if (r->readcount != 1) {
                *phase = 3;
                continue;
            }
            if (pv_wait(w, who, r->wmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 3;
            return PV_YIELD;
        case 3:
            pv_V(w, who, r->rmutex);
            *phase = 4;
            return PV_YIELD;
        case 4:
            pv_work(w, who, "read");
            *phase = 5;
            return PV_YIELD;
        default:
            return PV_YIELD;
        }
    }
}

static int reader_out(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 5:
            if (pv_wait(w, who, r->rmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 6;
            return PV_YIELD;
        case 6:
            r->readcount--;
            pv_count(w, who, "readcount", r->readcount);
            *phase = 7;
            return PV_YIELD;
        case 7:
            if (r->readcount != 0) {
                *phase = 8;
                continue;
            }
            pv_V(w, who, r->wmutex);
            *phase = 8;
            return PV_YIELD;
        case 8:
            pv_V(w, who, r->rmutex);
            *phase = 9;
            return PV_YIELD;
        default:
            return PV_DONE;
        }
    }
}

static int reader_pri(PvWorld *w, Rw *r, int who) {
    if (r->phase[who] <= 4) {
        return reader_in(w, r, who);
    }
    return reader_out(w, r, who);
}

static int writer_only(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    switch (*phase) {
    case 0:
        if (pv_wait(w, who, r->wmutex, mark)) {
            return PV_BLOCK;
        }
        *phase = 1;
        return PV_YIELD;
    case 1:
        pv_work(w, who, "write");
        *phase = 2;
        return PV_YIELD;
    case 2:
        pv_V(w, who, r->wmutex);
        *phase = 3;
        return PV_YIELD;
    default:
        return PV_DONE;
    }
}

static int pri_turn(PvWorld *w, Rw *r, int who) {
    if (who == r->writer) {
        return writer_only(w, r, who);
    }
    return reader_pri(w, r, who);
}

static int until_phase(PvWorld *w, Rw *r, int who, int target) {
    int guard = 0;
    while (r->phase[who] != target) {
        if (pri_turn(w, r, who) != PV_YIELD) {
            return -1;
        }
        if (++guard > 24) {
            return -1;
        }
    }
    return 0;
}

static int until_done(PvWorld *w, Rw *r, int who) {
    int guard = 0;
    for (;;) {
        int rc = pri_turn(w, r, who);
        if (rc == PV_DONE) {
            w->proc[who].done = 1;
            return 0;
        }
        if (rc != PV_YIELD || ++guard > 24) {
            return -1;
        }
    }
}

static void rw_summary(PvWorld *w, Rw *r, const char *second, Scene *out) {
    pv_print(w);
    printf("summary second=%s max_read=%d deadlock=%s\n", second, r->max_read,
           w->deadlock ? "yes" : "no");
    if (out != NULL) {
        out->max_read = r->max_read;
        out->deadlock = w->deadlock;
        out->cut = strcmp(second, "enter") == 0 ? 1 : 2;
    }
}

static int scene_reader_cut(Scene *out) {
    PvWorld w;
    Rw r;
    pv_init(&w);
    memset(&r, 0, sizeof r);
    r.writer = 1;
    r.rmutex = pv_sem(&w, "rmutex", 1);
    r.wmutex = pv_sem(&w, "wmutex", 1);
    pv_proc(&w, "R0");
    pv_proc(&w, "W0");
    pv_proc(&w, "R1");
    printf("problem=rw kind=reader-cut\n");
    pv_print_sems(&w);
    if (until_phase(&w, &r, 0, 5) != 0 || pri_turn(&w, &r, 1) != PV_BLOCK) {
        printf("script=stuck\n");
        return -1;
    }
    if (until_phase(&w, &r, 2, 5) != 0 || r.readcount != 2) {
        printf("script=stuck\n");
        return -1;
    }
    if (until_done(&w, &r, 0) != 0 || until_done(&w, &r, 2) != 0 ||
        until_done(&w, &r, 1) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    rw_summary(&w, &r, "enter", out);
    return 0;
}

static int scene_readers(Scene *out) {
    PvWorld w;
    Rw r;
    pv_init(&w);
    memset(&r, 0, sizeof r);
    r.writer = -1;
    r.rmutex = pv_sem(&w, "rmutex", 1);
    r.wmutex = pv_sem(&w, "wmutex", 1);
    pv_proc(&w, "R0");
    pv_proc(&w, "R1");
    printf("problem=rw kind=readers\n");
    pv_print_sems(&w);
    if (until_phase(&w, &r, 0, 5) != 0 || until_phase(&w, &r, 1, 5) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    if (until_done(&w, &r, 0) != 0 || until_done(&w, &r, 1) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    rw_summary(&w, &r, "enter", out);
    if (out != NULL) {
        out->cut = r.max_read >= 2 ? 1 : 0;
    }
    return 0;
}

int scene_rw(const char *kind, Scene *out) {
    scene_clear(out);
    if (strcmp(kind, "reader-cut") == 0) {
        return scene_reader_cut(out);
    }
    if (strcmp(kind, "writer-cut") == 0) {
        return scene_writer_cut(out);
    }
    if (strcmp(kind, "readers") == 0) {
        return scene_readers(out);
    }
    printf("%s unknown\n", kind);
    return -1;
}

