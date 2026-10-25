#include "smoke.h"

#include <stdio.h>
#include <string.h>

static const char *NAME[] = {"glue", "paper", "tobacco"};

static void print_offers(const int *code, int n) {
    int i;
    printf("offers=");
    for (i = 0; i < n; i++) {
        printf("%s%d", i ? "," : "", code[i]);
    }
    printf("\n");
}

static int fill_smoke(PvProg *prog, const int *code, int n, const int *offer, int finish,
                      const int *need) {
    int i;
    for (i = 0; i < n; i++) {
        if (pv_prog_add(&prog[0], PV_OP_V, offer[code[i] - 1], "-") != 0) {
            return -1;
        }
        if (pv_prog_add(&prog[0], PV_OP_P, finish, "-") != 0) {
            return -1;
        }
    }
    for (i = 0; i < 3; i++) {
        int k;
        for (k = 0; k < need[i]; k++) {
            if (pv_prog_add(&prog[i + 1], PV_OP_P, offer[i], "-") != 0 ||
                pv_prog_add(&prog[i + 1], PV_OP_WORK, -1, NAME[i]) != 0 ||
                pv_prog_add(&prog[i + 1], PV_OP_V, finish, "-") != 0) {
                return -1;
            }
        }
    }
    return 0;
}

int scene_smoke(const int *code, int n, Scene *out) {
    PvWorld w;
    PvProg prog[4];
    int offer[3];
    int finish;
    int need[3] = {0, 0, 0};
    int i;
    int smoked;
    scene_clear(out);
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    for (i = 0; i < n; i++) {
        need[code[i] - 1]++;
    }
    offer[0] = pv_sem(&w, "glue", 0);
    offer[1] = pv_sem(&w, "paper", 0);
    offer[2] = pv_sem(&w, "tobacco", 0);
    finish = pv_sem(&w, "finish", 0);
    pv_proc(&w, "agent");
    pv_proc(&w, "glue");
    pv_proc(&w, "paper");
    pv_proc(&w, "tobacco");
    if (fill_smoke(prog, code, n, offer, finish, need) != 0) {
        return -1;
    }
    printf("problem=smoke kind=combo\n");
    print_offers(code, n);
    pv_print_sems(&w);
    pv_sched(&w, prog, pv_exec);
    smoked = pv_count_note(&w, "glue") + pv_count_note(&w, "paper") +
             pv_count_note(&w, "tobacco");
    pv_print(&w);
    printf("summary smoked=%d mismatch=%s deadlock=%s\n", smoked,
           smoked == n && !w.deadlock ? "no" : "yes", w.deadlock ? "yes" : "no");
    if (w.deadlock) {
        pv_print_waits(&w);
    }
    if (out != NULL) {
        out->smoked = smoked;
        out->deadlock = w.deadlock;
        out->mismatch = smoked == n && !w.deadlock ? 0 : 1;
        out->too_long = w.too_long;
    }
    return w.too_long ? -1 : 0;
}

int scene_smoke_split(Scene *out) {
    PvWorld w;
    int tobacco;
    int paper;
    int glue;
    int finish;
    int agent;
    int sglue;
    int spaper;
    int stobacco;
    scene_clear(out);
    pv_init(&w);
    tobacco = pv_sem(&w, "tobacco", 0);
    paper = pv_sem(&w, "paper", 0);
    glue = pv_sem(&w, "glue", 0);
    finish = pv_sem(&w, "finish", 0);
    agent = pv_proc(&w, "agent");
    sglue = pv_proc(&w, "glue");
    spaper = pv_proc(&w, "paper");
    stobacco = pv_proc(&w, "tobacco");
    printf("problem=smoke kind=split\n");
    pv_print_sems(&w);
    /* 供应者放下烟草和纸。有纸的人抢走烟草，有烟草的人抢走纸。 */
    pv_V(&w, agent, tobacco);
    pv_P(&w, spaper, tobacco);
    pv_V(&w, agent, paper);
    pv_P(&w, stobacco, paper);
    pv_P(&w, sglue, tobacco);
    pv_P(&w, spaper, glue);
    pv_P(&w, stobacco, glue);
    pv_P(&w, agent, finish);
    (void)glue;
    (void)finish;
    w.deadlock = 1;
    pv_print(&w);
    printf("summary smoked=0 mismatch=yes deadlock=yes\n");
    pv_print_waits(&w);
    if (out != NULL) {
        out->deadlock = 1;
        out->mismatch = 1;
        out->smoked = 0;
    }
    return 0;
}
