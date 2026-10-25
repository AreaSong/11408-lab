#include "phi.h"

#include <stdio.h>
#include <string.h>

enum { SCH_LIMIT = 1, SCH_ODD, SCH_BOTH, SCH_BOWL };

typedef struct {
    int scheme;
    int n;
    int room;
    int pick;
    int chop[PV_MAX_PROC];
    int phase[PV_MAX_PROC];
    int mark[PV_MAX_PROC];
} Phi;

static int first_stick(const Phi *p, int who) {
    int left = p->chop[who];
    int right = p->chop[(who + 1) % p->n];
    /* 编号从 0 起。偶数先右后左，奇数先左后右，左右两批人不会围成环。 */
    if (p->scheme == SCH_ODD && who % 2 == 0) {
        return right;
    }
    return left;
}

static int second_stick(const Phi *p, int who) {
    int left = p->chop[who];
    int right = p->chop[(who + 1) % p->n];
    if (p->scheme == SCH_ODD && who % 2 == 0) {
        return left;
    }
    return right;
}

static int phi_gate(PvWorld *w, Phi *p, int who) {
    int *phase = &p->phase[who];
    int *mark = &p->mark[who];
    for (;;) {
        switch (*phase) {
        case 0:
            if (p->pick < 0) {
                *phase = 1;
                continue;
            }
            if (pv_wait(w, who, p->pick, mark)) {
                return PV_BLOCK;
            }
            *phase = 1;
            return PV_YIELD;
        case 1:
            if (p->room < 0) {
                *phase = 2;
                continue;
            }
            if (pv_wait(w, who, p->room, mark)) {
                return PV_BLOCK;
            }
            *phase = 2;
            return PV_YIELD;
        case 2:
            if (pv_wait(w, who, first_stick(p, who), mark)) {
                return PV_BLOCK;
            }
            *phase = 3;
            return PV_YIELD;
        case 3:
            if (pv_wait(w, who, second_stick(p, who), mark)) {
                return PV_BLOCK;
            }
            *phase = 4;
            return PV_YIELD;
        default:
            return PV_YIELD;
        }
    }
}

static int phi_leave(PvWorld *w, Phi *p, int who) {
    int *phase = &p->phase[who];
    for (;;) {
        switch (*phase) {
        case 4:
            if (p->pick < 0) {
                *phase = 5;
                continue;
            }
            pv_V(w, who, p->pick);
            *phase = 5;
            return PV_YIELD;
        case 5:
            pv_work(w, who, "eat");
            *phase = 6;
            return PV_YIELD;
        case 6:
            pv_V(w, who, p->chop[who]);
            *phase = 7;
            return PV_YIELD;
        case 7:
            pv_V(w, who, p->chop[(who + 1) % p->n]);
            *phase = 8;
            return PV_YIELD;
        case 8:
            if (p->room < 0) {
                return PV_DONE;
            }
            pv_V(w, who, p->room);
            *phase = 9;
            return PV_YIELD;
        default:
            return PV_DONE;
        }
    }
}

static int phi_turn(PvWorld *w, void *ctx, int who) {
    Phi *p = ctx;
    if (p->phase[who] <= 3) {
        return phi_gate(w, p, who);
    }
    return phi_leave(w, p, who);
}

static void add_people(PvWorld *w, Phi *p, int n) {
    int i;
    p->n = n;
    p->room = -1;
    p->pick = -1;
    for (i = 0; i < n; i++) {
        char name[PV_NAME];
        snprintf(name, sizeof name, "chop%d", i);
        p->chop[i] = pv_sem(w, name, 1);
    }
    for (i = 0; i < n; i++) {
        char name[PV_NAME];
        snprintf(name, sizeof name, "P%d", i);
        pv_proc(w, name);
    }
}

static void finish_phi(PvWorld *w, Scene *out) {
    int meals = pv_count_note(w, "eat");
    pv_print(w);
    printf("summary meals=%d deadlock=%s\n", meals, w->deadlock ? "yes" : "no");
    if (w->deadlock) {
        pv_print_waits(w);
    }
    if (out != NULL) {
        out->meals = meals;
        out->deadlock = w->deadlock;
        out->too_long = w->too_long;
    }
}

static int scene_dead(int n, Scene *out) {
    PvWorld w;
    Phi p;
    int i;
    int bad = 0;
    pv_init(&w);
    memset(&p, 0, sizeof p);
    add_people(&w, &p, n);
    printf("problem=phi kind=dead n=%d\n", n);
    pv_print_sems(&w);
    for (i = 0; i < n; i++) {
        if (!pv_P(&w, i, p.chop[i])) {
            bad = 1;
        }
    }
    for (i = 0; i < n; i++) {
        if (pv_P(&w, i, p.chop[(i + 1) % n])) {
            bad = 1;
        }
    }
    w.deadlock = 1;
    finish_phi(&w, out);
    if (out != NULL && bad) {
        out->mismatch = 1;
    }
    return bad ? -1 : 0;
}

static int scene_scheme(const char *kind, int scheme, int n, Scene *out) {
    PvWorld w;
    Phi p;
    pv_init(&w);
    memset(&p, 0, sizeof p);
    p.scheme = scheme;
    add_people(&w, &p, n);
    if (scheme == SCH_LIMIT) {
        p.room = pv_sem(&w, "room", n - 1);
    } else if (scheme == SCH_BOTH) {
        p.pick = pv_sem(&w, "pick", 1);
    }
    printf("problem=phi kind=%s n=%d\n", kind, n);
    pv_print_sems(&w);
    pv_sched(&w, &p, phi_turn);
    finish_phi(&w, out);
    return w.too_long ? -1 : 0;
}

int scene_phi(const char *kind, int n, Scene *out) {
    scene_clear(out);
    if (strcmp(kind, "dead") == 0) {
        return scene_dead(n, out);
    }
    if (strcmp(kind, "limit") == 0) {
        return scene_scheme(kind, SCH_LIMIT, n, out);
    }
    if (strcmp(kind, "odd") == 0) {
        return scene_scheme(kind, SCH_ODD, n, out);
    }
    if (strcmp(kind, "both") == 0) {
        return scene_scheme(kind, SCH_BOTH, n, out);
    }
    printf("%s unknown\n", kind);
    return -1;
}

int scene_2019(int n, int m, Scene *out) {
    PvWorld w;
    Phi p;
    int bowl = n - 1 < m ? n - 1 : m;
    scene_clear(out);
    pv_init(&w);
    memset(&p, 0, sizeof p);
    p.scheme = SCH_BOWL;
    add_people(&w, &p, n);
    p.room = pv_sem(&w, "bowl", bowl);
    printf("problem=exam year=2019 n=%d m=%d\n", n, m);
    pv_print_sems(&w);
    pv_sched(&w, &p, phi_turn);
    finish_phi(&w, out);
    if (out != NULL) {
        out->bowl = bowl;
    }
    return w.too_long ? -1 : 0;
}
