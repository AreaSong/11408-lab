#include "exam.h"

#include <stdio.h>
#include <string.h>

static int pos_of(char names[][PV_NAME], int n, const char *want) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(names[i], want) == 0) {
            return i;
        }
    }
    return -1;
}

static int order_ok(char names[][PV_NAME], int n) {
    int a = pos_of(names, n, "A");
    int b = pos_of(names, n, "B");
    int c = pos_of(names, n, "C");
    int d = pos_of(names, n, "D");
    int e = pos_of(names, n, "E");
    int f = pos_of(names, n, "F");
    if (a < 0 || b < 0 || c < 0 || d < 0 || e < 0 || f < 0) {
        return 0;
    }
    if (!(a < c && b < c && c < d && c < e && e < f)) {
        return 0;
    }
    return a < e && e < f && b < c && c < d;
}

static void join_works(const PvWorld *w, char *buf, size_t nbuf) {
    char names[8][PV_NAME];
    int n = pv_works(w, names, 8);
    size_t used = 0;
    int i;
    buf[0] = '\0';
    for (i = 0; i < n; i++) {
        int wrote = snprintf(buf + used, nbuf - used, "%s%s", i ? "," : "", names[i]);
        if (wrote < 0 || (size_t)wrote >= nbuf - used) {
            return;
        }
        used += (size_t)wrote;
    }
}

int scene_2022(Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int ac;
    int ce;
    char order[64];
    char names[8][PV_NAME];
    int nworks;
    int ok;
    scene_clear(out);
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    ac = pv_sem(&w, "AC", 0);
    ce = pv_sem(&w, "CE", 0);
    pv_proc(&w, "T1");
    pv_proc(&w, "T2");
    pv_prog_add(&prog[0], PV_OP_WORK, -1, "A");
    pv_prog_add(&prog[0], PV_OP_V, ac, "-");
    pv_prog_add(&prog[0], PV_OP_P, ce, "-");
    pv_prog_add(&prog[0], PV_OP_WORK, -1, "E");
    pv_prog_add(&prog[0], PV_OP_WORK, -1, "F");
    pv_prog_add(&prog[1], PV_OP_WORK, -1, "B");
    pv_prog_add(&prog[1], PV_OP_P, ac, "-");
    pv_prog_add(&prog[1], PV_OP_WORK, -1, "C");
    pv_prog_add(&prog[1], PV_OP_V, ce, "-");
    pv_prog_add(&prog[1], PV_OP_WORK, -1, "D");
    printf("problem=exam year=2022\n");
    printf("thread T1=A,E,F\n");
    printf("thread T2=B,C,D\n");
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    nworks = pv_works(&w, names, 8);
    ok = order_ok(names, nworks) && !w.deadlock;
    join_works(&w, order, sizeof order);
    pv_print(&w);
    printf("summary order=%s check=%s deadlock=%s\n", order, ok ? "yes" : "no",
           w.deadlock ? "yes" : "no");
    if (out != NULL) {
        out->check = ok;
        out->deadlock = w.deadlock;
        out->too_long = w.too_long;
    }
    return w.too_long ? -1 : 0;
}

static int add_once(PvProg *p, int sem_p, int sem_v, const char *work) {
    if (sem_p >= 0 && pv_prog_add(p, PV_OP_P, sem_p, "-") != 0) {
        return -1;
    }
    if (pv_prog_add(p, PV_OP_WORK, -1, work) != 0) {
        return -1;
    }
    if (sem_v >= 0 && pv_prog_add(p, PV_OP_V, sem_v, "-") != 0) {
        return -1;
    }
    return 0;
}

static void part2(Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int empty;
    int full;
    char order[32];
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    empty = pv_sem(&w, "empty", 1);
    full = pv_sem(&w, "full", 0);
    pv_proc(&w, "P1");
    pv_proc(&w, "P2");
    add_once(&prog[0], empty, full, "C1");
    add_once(&prog[1], full, empty, "C2");
    printf("problem=exam year=2024 part=2\n");
    printf("mutex=none\n");
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    join_works(&w, order, sizeof order);
    pv_print(&w);
    printf("summary order=%s deadlock=%s\n", order, w.deadlock ? "yes" : "no");
    if (out != NULL && (w.deadlock || strcmp(order, "C1,C2") != 0)) {
        out->check = 0;
    }
}

static void part3(void) {
    PvWorld w;
    PvProg prog[2];
    int mutex;
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    mutex = pv_sem(&w, "mutex", 1);
    pv_proc(&w, "P1");
    pv_proc(&w, "P2");
    add_once(&prog[0], mutex, mutex, "C3");
    add_once(&prog[1], mutex, mutex, "C3");
    printf("problem=exam year=2024 part=3\n");
    printf("sync=none\n");
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    pv_print(&w);
    printf("summary order=C3,C3 deadlock=%s\n", w.deadlock ? "yes" : "no");
}

int scene_2024(Scene *out) {
    scene_clear(out);
    if (out != NULL) {
        out->check = 1;
    }
    printf("problem=exam year=2024 part=1\n");
    printf("critical=yes reason=both-write-B\n");
    part2(out);
    part3();
    return out != NULL && out->too_long ? -1 : 0;
}

static int scan_open(const PvWorld *w) {
    int i;
    int open = 0;
    int max = 0;
    for (i = 0; i < w->ntrace; i++) {
        if (strstr(w->trace[i], "note=dig") != NULL) {
            open++;
        }
        if (strstr(w->trace[i], "note=fill") != NULL) {
            open--;
        }
        if (open > max) {
            max = open;
        }
    }
    return max;
}

static void fill_plant(PvProg *prog, int trees, int pits, int empty, int water, int mutex) {
    int i;
    for (i = 0; i < trees; i++) {
        pv_prog_add(&prog[0], PV_OP_P, pits, "-");
        pv_prog_add(&prog[0], PV_OP_P, mutex, "-");
        pv_prog_add(&prog[0], PV_OP_WORK, -1, "dig");
        pv_prog_add(&prog[0], PV_OP_V, mutex, "-");
        pv_prog_add(&prog[0], PV_OP_V, empty, "-");
        pv_prog_add(&prog[1], PV_OP_P, empty, "-");
        pv_prog_add(&prog[1], PV_OP_P, mutex, "-");
        pv_prog_add(&prog[1], PV_OP_WORK, -1, "fill");
        pv_prog_add(&prog[1], PV_OP_V, mutex, "-");
        pv_prog_add(&prog[1], PV_OP_V, pits, "-");
        pv_prog_add(&prog[1], PV_OP_V, water, "-");
        pv_prog_add(&prog[2], PV_OP_P, water, "-");
        pv_prog_add(&prog[2], PV_OP_WORK, -1, "pour");
    }
}

int scene_2025(int trees, Scene *out) {
    PvWorld w;
    PvProg prog[3];
    int pits;
    int empty;
    int water;
    int mutex;
    int watered;
    int max_open;
    scene_clear(out);
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    pits = pv_sem(&w, "pits", 3);
    empty = pv_sem(&w, "empty", 0);
    water = pv_sem(&w, "water", 0);
    mutex = pv_sem(&w, "mutex", 1);
    pv_proc(&w, "jia");
    pv_proc(&w, "yi");
    pv_proc(&w, "bing");
    fill_plant(prog, trees, pits, empty, water, mutex);
    printf("problem=exam year=2025 trees=%d\n", trees);
    printf("bucket=none\n");
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    watered = pv_count_note(&w, "pour");
    max_open = scan_open(&w);
    pv_print(&w);
    printf("summary watered=%d max_open=%d deadlock=%s\n", watered, max_open,
           w.deadlock ? "yes" : "no");
    if (out != NULL) {
        out->watered = watered;
        out->max_open = max_open;
        out->deadlock = w.deadlock;
        out->too_long = w.too_long;
        out->check = !w.deadlock && watered == trees && max_open <= 3 && max_open > 0;
    }
    return w.too_long ? -1 : 0;
}
