#include "prec.h"

#include <stdio.h>
#include <string.h>

#define PREC_MAX_N 6

static int prec_bad(int n, int m, const int *u, const int *v) {
    int i;
    int j;
    if (n < 2 || n > PREC_MAX_N || m < 0 || m > PV_MAX_SEM) {
        return 1;
    }
    for (i = 0; i < m; i++) {
        if (u[i] < 0 || u[i] >= n || v[i] < 0 || v[i] >= n || u[i] == v[i]) {
            return 1;
        }
        for (j = 0; j < i; j++) {
            if (u[j] == u[i] && v[j] == v[i]) {
                return 1;
            }
        }
    }
    return 0;
}

/* incoming=1 时按前驱编号从小到大 P；否则按后继编号从小到大 V。 */
static int add_ends(PvProg *prog, const int *eu, const int *ev, const int *sem, int m, int n,
                    int who, int incoming) {
    int other;
    for (other = 0; other < n; other++) {
        int k;
        for (k = 0; k < m; k++) {
            int hit = incoming ? (ev[k] == who && eu[k] == other) : (eu[k] == who && ev[k] == other);
            if (hit && pv_prog_add(prog, incoming ? PV_OP_P : PV_OP_V, sem[k], "-") != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int build_prog(PvProg *prog, const int *eu, const int *ev, const int *sem, int m, int n,
                      int who, const char *name) {
    memset(prog, 0, sizeof(*prog));
    if (add_ends(prog, eu, ev, sem, m, n, who, 1) != 0) {
        return -1;
    }
    if (pv_prog_add(prog, PV_OP_WORK, -1, name) != 0) {
        return -1;
    }
    return add_ends(prog, eu, ev, sem, m, n, who, 0);
}

static int work_pos(char works[][PV_NAME], int nworks, const char *name) {
    int i;
    for (i = 0; i < nworks; i++) {
        if (strcmp(works[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int order_ok(char works[][PV_NAME], int nworks, char names[][PV_NAME], const int *u,
                    const int *v, int m) {
    int k;
    for (k = 0; k < m; k++) {
        int from = work_pos(works, nworks, names[u[k]]);
        int to = work_pos(works, nworks, names[v[k]]);
        if (from < 0 || to < 0 || from >= to) {
            return 0;
        }
    }
    return 1;
}

static void join_works(char works[][PV_NAME], int nworks, char *buf, size_t nbuf) {
    size_t used = 0;
    int i;
    buf[0] = '\0';
    if (nworks == 0) {
        snprintf(buf, nbuf, "-");
        return;
    }
    for (i = 0; i < nworks; i++) {
        int wrote = snprintf(buf + used, nbuf - used, "%s%s", i ? "," : "", works[i]);
        if (wrote < 0 || (size_t)wrote >= nbuf - used) {
            return;
        }
        used += (size_t)wrote;
    }
}

static int scene_prec_run(const char *problem, int n, char names[][PV_NAME], const int *u,
                          const int *v, char sem_name[][PV_NAME], int m, Scene *out) {
    PvWorld w;
    PvProg prog[PREC_MAX_N];
    int sem[PV_MAX_SEM];
    char works[PREC_MAX_N][PV_NAME];
    char order[128];
    int nworks;
    int ok;
    int i;
    scene_clear(out);
    pv_init(&w);
    printf("%s\n", problem);
    for (i = 0; i < m; i++) {
        printf("edge %s-%s sem=%s\n", names[u[i]], names[v[i]], sem_name[i]);
        sem[i] = pv_sem(&w, sem_name[i], 0);
    }
    for (i = 0; i < n; i++) {
        pv_proc(&w, names[i]);
        if (build_prog(&prog[i], u, v, sem, m, n, i, names[i]) != 0) {
            return -1;
        }
    }
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    nworks = pv_works(&w, works, PREC_MAX_N);
    ok = !w.deadlock && nworks == n && order_ok(works, nworks, names, u, v, m);
    join_works(works, nworks, order, sizeof order);
    pv_print(&w);
    printf("summary done=%d order=%s check=%s deadlock=%s\n", nworks, order, ok ? "yes" : "no",
           w.deadlock ? "yes" : "no");
    if (w.deadlock) {
        pv_print_waits(&w);
    }
    if (out != NULL) {
        out->produced = nworks;
        out->check = ok;
        out->deadlock = w.deadlock;
        out->too_long = w.too_long;
    }
    return w.too_long ? -1 : 0;
}

int scene_prec(int n, const int *u, const int *v, int m, Scene *out) {
    char names[PREC_MAX_N][PV_NAME];
    char sem_name[PV_MAX_SEM][PV_NAME];
    char problem[64];
    int i;
    if (u == NULL || v == NULL || prec_bad(n, m, u, v)) {
        return -2;
    }
    for (i = 0; i < n; i++) {
        snprintf(names[i], PV_NAME, "P%d", i);
    }
    for (i = 0; i < m; i++) {
        snprintf(sem_name[i], PV_NAME, "e%d_%d", u[i], v[i]);
    }
    snprintf(problem, sizeof problem, "problem=prec n=%d edges=%d", n, m);
    return scene_prec_run(problem, n, names, u, v, sem_name, m, out);
}

int scene_2020(Scene *out) {
    char names[5][PV_NAME];
    char sem_name[4][PV_NAME];
    int u[4] = {0, 1, 2, 3};
    int v[4] = {2, 2, 4, 4};
    const char *node[] = {"A", "B", "C", "D", "E"};
    const char *edge[] = {"AC", "BC", "CE", "DE"};
    int i;
    for (i = 0; i < 5; i++) {
        snprintf(names[i], PV_NAME, "%s", node[i]);
    }
    for (i = 0; i < 4; i++) {
        snprintf(sem_name[i], PV_NAME, "%s", edge[i]);
    }
    return scene_prec_run("problem=exam year=2020", 5, names, u, v, sem_name, 4, out);
}
