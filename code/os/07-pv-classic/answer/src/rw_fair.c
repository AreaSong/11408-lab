#include "rw.h"
#include "rw_state.h"

#include <stdio.h>
#include <string.h>

/*
 * 考纲：读者-写者的读写公平。王道的第三种是在读者优先上再加一把 w。
 * 读者和写者都先 P(w)，所以申请按到达顺序排队，后来的读者插不到已经在等的写者前面。
 * 读者登记完就 V(w)，读的时候不占着 w；没有写者排队时，后面的读者仍能一起读。
 * 写者要写完才 V(w)，这段时间后来的读者和写者都排在 w 上。
 */

static void fair_summary(PvWorld *w, Rw *r, const char *second, Scene *out) {
    pv_print(w);
    printf("summary second=%s max_read=%d deadlock=%s\n", second, r->max_read,
           w->deadlock ? "yes" : "no");
    if (out != NULL) {
        out->max_read = r->max_read;
        out->deadlock = w->deadlock;
        out->cut = strcmp(second, "enter") == 0 ? 1 : 2;
    }
}

static int fair_reader_apply(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    for (;;) {
        switch (*phase) {
        case 0:
            if (pv_wait(w, who, r->wsem, mark)) {
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
            /* 不是第一个读者就不必再抢文件，这一步不单独占一次调度。 */
            if (r->readcount != 1) {
                *phase = 4;
                continue;
            }
            if (pv_wait(w, who, r->wmutex, mark)) {
                return PV_BLOCK;
            }
            *phase = 4;
            return PV_YIELD;
        case 4:
            pv_V(w, who, r->rmutex);
            *phase = 5;
            return PV_YIELD;
        default:
            return PV_YIELD;
        }
    }
}

static int fair_reader_go(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    switch (*phase) {
    case 5:
        /* 申请结束就放开 w，队头的下一个人（读者或写者）才能继续申请。 */
        pv_V(w, who, r->wsem);
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

static int fair_reader_leave(PvWorld *w, Rw *r, int who) {
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
            pv_V(w, who, r->wmutex);
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

static int fair_reader(PvWorld *w, Rw *r, int who) {
    if (r->phase[who] <= 4) {
        return fair_reader_apply(w, r, who);
    }
    if (r->phase[who] <= 6) {
        return fair_reader_go(w, r, who);
    }
    return fair_reader_leave(w, r, who);
}

static int fair_writer(PvWorld *w, Rw *r, int who) {
    int *phase = &r->phase[who];
    int *mark = &r->mark[who];
    switch (*phase) {
    case 0:
        if (pv_wait(w, who, r->wsem, mark)) {
            return PV_BLOCK;
        }
        *phase = 1;
        return PV_YIELD;
    case 1:
        if (pv_wait(w, who, r->wmutex, mark)) {
            return PV_BLOCK;
        }
        *phase = 2;
        return PV_YIELD;
    case 2:
        pv_work(w, who, "write");
        *phase = 3;
        return PV_YIELD;
    case 3:
        pv_V(w, who, r->wmutex);
        *phase = 4;
        return PV_YIELD;
    case 4:
        pv_V(w, who, r->wsem);
        *phase = 5;
        return PV_YIELD;
    default:
        return PV_DONE;
    }
}

static int fair_turn(PvWorld *w, Rw *r, int who) {
    if (who == r->writer) {
        return fair_writer(w, r, who);
    }
    return fair_reader(w, r, who);
}

static int fair_until(PvWorld *w, Rw *r, int who, int target) {
    int guard = 0;
    while (r->phase[who] != target) {
        if (fair_turn(w, r, who) != PV_YIELD) {
            return -1;
        }
        if (++guard > 24) {
            return -1;
        }
    }
    return 0;
}

static int fair_done(PvWorld *w, Rw *r, int who) {
    int guard = 0;
    for (;;) {
        int rc = fair_turn(w, r, who);
        if (rc == PV_DONE) {
            w->proc[who].done = 1;
            return 0;
        }
        if (rc != PV_YIELD || ++guard > 24) {
            return -1;
        }
    }
}

static int fair_boot(PvWorld *w, Rw *r, int with_writer) {
    pv_init(w);
    memset(r, 0, sizeof *r);
    r->writer = with_writer ? 1 : -1;
    r->rmutex = pv_sem(w, "rmutex", 1);
    r->wmutex = pv_sem(w, "wmutex", 1);
    r->wsem = pv_sem(w, "w", 1);
    pv_proc(w, "R0");
    if (with_writer) {
        pv_proc(w, "W0");
    }
    pv_proc(w, "R1");
    return 0;
}

static int scene_fair_cut(Scene *out) {
    PvWorld w;
    Rw r;
    int guard = 0;
    fair_boot(&w, &r, 1);
    printf("problem=rw kind=fair-cut\n");
    pv_print_sems(&w);
    if (fair_until(&w, &r, 0, 7) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    while (w.proc[1].blocked < 0) {
        if (fair_turn(&w, &r, 1) == PV_DONE || ++guard > 24) {
            printf("script=stuck\n");
            return -1;
        }
    }
    if (fair_turn(&w, &r, 2) != PV_BLOCK) {
        printf("script=stuck\n");
        return -1;
    }
    if (fair_done(&w, &r, 0) != 0 || fair_done(&w, &r, 1) != 0 || fair_done(&w, &r, 2) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    fair_summary(&w, &r, "block", out);
    return 0;
}

static int scene_fair_readers(Scene *out) {
    PvWorld w;
    Rw r;
    fair_boot(&w, &r, 0);
    printf("problem=rw kind=fair-readers\n");
    pv_print_sems(&w);
    if (fair_until(&w, &r, 0, 7) != 0 || fair_until(&w, &r, 1, 7) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    if (fair_done(&w, &r, 0) != 0 || fair_done(&w, &r, 1) != 0) {
        printf("script=stuck\n");
        return -1;
    }
    fair_summary(&w, &r, "enter", out);
    return 0;
}

int scene_fair(const char *kind, Scene *out) {
    if (strcmp(kind, "fair-cut") == 0) {
        return scene_fair_cut(out);
    }
    if (strcmp(kind, "fair-readers") == 0) {
        return scene_fair_readers(out);
    }
    printf("%s unknown\n", kind);
    return -1;
}
