#include "sim.h"

#include <string.h>

void step_blank(Step *s, int frames) {
    int i;

    memset(s, 0, sizeof *s);
    s->victim = -1;
    s->hand = -1;
    for (i = 0; i < frames; i++) {
        s->slots[i] = -1;
        s->refbit[i] = -1;
        s->dirty[i] = -1;
        s->order[i] = -1;
    }
}

int algo_id(const char *name) {
    if (strcmp(name, "fifo") == 0) {
        return 0;
    }
    if (strcmp(name, "lru") == 0) {
        return 1;
    }
    if (strcmp(name, "opt") == 0) {
        return 2;
    }
    if (strcmp(name, "clock") == 0) {
        return 3;
    }
    if (strcmp(name, "clock2") == 0) {
        return 4;
    }
    return -1;
}

const char *algo_name(int id) {
    static const char *names[] = {"fifo", "lru", "opt", "clock", "clock2"};
    if (id < 0 || id > 4) {
        return "";
    }
    return names[id];
}
