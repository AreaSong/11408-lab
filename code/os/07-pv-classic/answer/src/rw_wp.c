#include "rw.h"
#include "rw_state.h"

#include <stdio.h>
#include <string.h>

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

static int wp_reader_in(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 0:
            if (pv_wait(w, who, r->door, mark)) {
                return PV_BLOCK;
            }
            *phase = 1;
            return PV_YIELD;
        case 1:
            if (pv_wait(w, who, r->rmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 2;
            return PV_YIELD;
        case 2:
            r->readcount++;
            if (r->readcount > r->max_read) {
                r->max_read = r->readcount;
            }
            pv_count(w, who, "readcount", r->readcount);
            *phase = 3;
            return PV_YIELD;
        case 3:
            if (r->readcount != 1) {
                *phase = 4;
                continue;
            }
            if (pv_wait(w, who, r->file, mark)) {
                return PV_BLOCK;
            }
            *phase = 4;
            return PV_YIELD;
        default:
            return PV_YIELD;
        }
    }
}

static int wp_reader_mid(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    switch (*phase) {
    case 4:
        pv_V(w, who, r->rmutex);
        *phase = 5;
        return PV_YIELD;
    case 5:
        pv_V(w, who, r->door);
        *phase = 6;
        return PV_YIELD;
    case 6:
        pv_work(w, who, "read");
        *phase = 7;
        return PV_YIELD;
    default:
        return PV_YIELD;
    }
}

static int wp_reader_out(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 7:
            if (pv_wait(w, who, r->rmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 8;
            return PV_YIELD;
        case 8:
            r->readcount--;
            pv_count(w, who, "readcount", r->readcount);
            *phase = 9;
            return PV_YIELD;
        case 9:
            if (r->readcount != 0) {
                *phase = 10;
                continue;
            }
            pv_V(w, who, r->file);
            *phase = 10;
            return PV_YIELD;
        case 10:
            pv_V(w, who, r->rmutex);
            *phase = 11;
            return PV_YIELD;
        default:
            return PV_DONE;
        }
    }
}

static int wp_reader(PvWorld *w, Rw *r, int who) {
    if (r->phase[who] <= 3) {
        return wp_reader_in(w, r, who);
    }
    if (r->phase[who] <= 6) {
        return wp_reader_mid(w, r, who);
    }
    return wp_reader_out(w, r, who);
}

static int wp_writer_in(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 0:
            if (pv_wait(w, who, r->wmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 1;
            return PV_YIELD;
        case 1:
            r->writecount++;
            pv_count(w, who, "writecount", r->writecount);
            *phase = 2;
            return PV_YIELD;
        case 2:
            if (r->writecount != 1) {
                *phase = 3;
                continue;
            }
            if (pv_wait(w, who, r->door, mark)) {
                return PV_BLOCK;
            }
            *phase = 3;
            return PV_YIELD;
        case 3:
            pv_V(w, who, r->wmutex);
            *phase = 4;
            return PV_YIELD;
        case 4:
            if (pv_wait(w, who, r->file, mark)) {
                return PV_BLOCK;
            }
            *phase = 5;
            return PV_YIELD;
        default:
            return PV_YIELD;
        }
    }
}

static int wp_writer_out(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 5:
            pv_work(w, who, "write");
            *phase = 6;
            return PV_YIELD;
        case 6:
            pv_V(w, who, r->file);
            *phase = 7;
            return PV_YIELD;
        case 7:
            if (pv_wait(w, who, r->wmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 8;
            return PV_YIELD;
        case 8:
            r->writecount--;
            pv_count(w, who, "writecount", r->writecount);
            *phase = 9;
            return PV_YIELD;
        case 9:
            if (r->writecount != 0) {
                *phase = 10;
                continue;
            }
            pv_V(w, who, r->door);
            *phase = 10;
            return PV_YIELD;
        case 10:
            pv_V(w, who, r->wmutex);
            *phase = 11;
            return PV_YIELD;
        default:
            return PV_DONE;
        }
    }
}

static int wp_writer(PvWorld *w, Rw *r, int who) {
    if (r->phase[who] <= 4) {
        return wp_writer_in(w, r, who);
    }
    return wp_writer_out(w, r, who);
}

static int wp_turn(PvWorld *w, Rw *r, int who) {
    if (who == 1) {
        return wp_writer(w, r, who);
    }
    return wp_reader(w, r, who);
}

static int wp_until(PvWorld *w, Rw *r, int who, int target) {
    int guard = 0;
    while (r->phase[who] != target) {
        if (wp_turn(w, r, who) != PV_YIELD) {
            return -1;
        }
        if (++guard > 24) {
            return -1;
        }
    }
    return 0;
}

static int wp_done(PvWorld *w, Rw *r, int who) {
    int guard = 0;
    for (;;) {
        int rc = wp_turn(w, r, who);
        if (rc == PV_DONE) {
            w->proc[who].done = 1;
            return 0;
        }
        if (rc != PV_YIELD || ++guard > 24) {
            return -1;
        }
    }
}

int scene_writer_cut(Scene *out) {
    PvWorld w;
    Rw r;
    int guard = 0;
    pv_init(&w);
    memset(&r, 0, sizeof r);
    r.rmutex = pv_sem(&w, "rmutex", 1);
    r.wmutex = pv_sem(&w, "wmutex", 1);
    r.file = pv_sem(&w, "file", 1);
    r.door = pv_sem(&w, "r", 1);
    pv_proc(&w, "R0");
    pv_proc(&w, "W0");
    pv_proc(&w, "R1");
    printf("problem=rw kind=writer-cut\n");
    pv_print_sems(&w);
    if (wp_until(&w, &r, 0, 7) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    while (w.proc[1].blocked < 0) {
        if (wp_turn(&w, &r, 1) == PV_DONE || ++guard > 24) {
            printf("script=stuck\n");
            return -1;
        }
    }
    if (wp_turn(&w, &r, 2) != PV_BLOCK) {
        printf("script=stuck\n");
        return -1;
    }
    if (wp_done(&w, &r, 0) != 0 || wp_done(&w, &r, 1) != 0 || wp_done(&w, &r, 2) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    rw_summary(&w, &r, "block", out);
    return 0;
}

