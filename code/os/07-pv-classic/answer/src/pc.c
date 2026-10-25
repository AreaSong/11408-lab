#include "pc.h"

#include <stdio.h>
#include <string.h>

static int add_cycle(PvProg *p, int first, int second, int v1, int v2, const char *work) {
    if (pv_prog_add(p, PV_OP_P, first, "-") != 0) {
        return -1;
    }
    if (pv_prog_add(p, PV_OP_P, second, "-") != 0) {
        return -1;
    }
    if (pv_prog_add(p, PV_OP_WORK, -1, work) != 0) {
        return -1;
    }
    if (pv_prog_add(p, PV_OP_V, v1, "-") != 0) {
        return -1;
    }
    return pv_prog_add(p, PV_OP_V, v2, "-");
}

static int run(PvWorld *w, PvProg *prog, Scene *out, int count_items) {
    int produced = 0;
    int consumed = 0;
    pv_print_sems(w);
    pv_sched(w, prog, pv_exec);
    if (count_items) {
        produced = pv_count_note(w, "put");
        consumed = pv_count_note(w, "get");
    }
    pv_print(w);
    if (count_items) {
        printf("summary produced=%d consumed=%d deadlock=%s\n", produced, consumed,
               w->deadlock ? "yes" : "no");
    } else {
        printf("summary deadlock=%s\n", w->deadlock ? "yes" : "no");
    }
    if (w->deadlock) {
        pv_print_waits(w);
    }
    if (out != NULL) {
        out->produced = produced;
        out->consumed = consumed;
        out->deadlock = w->deadlock;
        out->too_long = w->too_long;
    }
    return w->too_long ? -1 : 0;
}

int scene_pc(int cap, int items, Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int empty;
    int full;
    int mutex;
    int i;
    scene_clear(out);
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    empty = pv_sem(&w, "empty", cap);
    full = pv_sem(&w, "full", 0);
    mutex = pv_sem(&w, "mutex", 1);
    pv_proc(&w, "P");
    pv_proc(&w, "C");
    for (i = 0; i < items; i++) {
        if (add_cycle(&prog[0], empty, mutex, mutex, full, "put") != 0) {
            return -1;
        }
        if (add_cycle(&prog[1], full, mutex, mutex, empty, "get") != 0) {
            return -1;
        }
    }
    printf("problem=pc cap=%d items=%d\n", cap, items);
    return run(&w, prog, out, 1);
}

static int scene_right(Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int empty = 0;
    int full = 0;
    int mutex = 0;
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    empty = pv_sem(&w, "empty", 1);
    full = pv_sem(&w, "full", 0);
    mutex = pv_sem(&w, "mutex", 1);
    /* 消费者编号更小，所以会先去等产品，此时还没拿互斥锁。 */
    pv_proc(&w, "C");
    pv_proc(&w, "P");
    if (add_cycle(&prog[0], full, mutex, mutex, empty, "get") != 0) {
        return -1;
    }
    if (add_cycle(&prog[1], empty, mutex, mutex, full, "put") != 0) {
        return -1;
    }
    printf("problem=order kind=right cap=1\n");
    return run(&w, prog, out, 1);
}

static int scene_wrong(int full_start, Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int empty;
    int full;
    int mutex;
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    empty = pv_sem(&w, "empty", full_start ? 0 : 1);
    full = pv_sem(&w, "full", full_start ? 1 : 0);
    mutex = pv_sem(&w, "mutex", 1);
    if (full_start) {
        pv_proc(&w, "P");
        pv_proc(&w, "C");
        if (add_cycle(&prog[0], mutex, empty, mutex, full, "put") != 0) {
            return -1;
        }
        if (add_cycle(&prog[1], mutex, full, mutex, empty, "get") != 0) {
            return -1;
        }
        printf("problem=order kind=wrong-full cap=1\n");
    } else {
        pv_proc(&w, "C");
        pv_proc(&w, "P");
        if (add_cycle(&prog[0], mutex, full, mutex, empty, "get") != 0) {
            return -1;
        }
        if (add_cycle(&prog[1], mutex, empty, mutex, full, "put") != 0) {
            return -1;
        }
        printf("problem=order kind=wrong cap=1\n");
    }
    return run(&w, prog, out, 1);
}

/* 已经有 3 个坑时，甲若先拿铁锹再等名额，乙拿不到铁锹去填，名额不会回来。 */
static int scene_plant(Scene *out) {
    PvWorld w;
    PvProg prog[2];
    int pits;
    int empty;
    int mutex;
    pv_init(&w);
    memset(prog, 0, sizeof prog);
    pits = pv_sem(&w, "pits", 0);
    empty = pv_sem(&w, "empty", 3);
    mutex = pv_sem(&w, "mutex", 1);
    pv_sem(&w, "water", 0);
    pv_proc(&w, "jia");
    pv_proc(&w, "yi");
    if (add_cycle(&prog[0], mutex, pits, mutex, empty, "dig") != 0) {
        return -1;
    }
    if (pv_prog_add(&prog[1], PV_OP_P, empty, "-") != 0) {
        return -1;
    }
    if (pv_prog_add(&prog[1], PV_OP_P, mutex, "-") != 0) {
        return -1;
    }
    printf("problem=order kind=plant\n");
    printf("open=3\n");
    return run(&w, prog, out, 0);
}

int scene_order(const char *kind, Scene *out) {
    scene_clear(out);
    if (strcmp(kind, "right") == 0) {
        return scene_right(out);
    }
    if (strcmp(kind, "wrong") == 0) {
        return scene_wrong(0, out);
    }
    if (strcmp(kind, "wrong-full") == 0) {
        return scene_wrong(1, out);
    }
    if (strcmp(kind, "plant") == 0) {
        return scene_plant(out);
    }
    printf("%s unknown\n", kind);
    return -1;
}
