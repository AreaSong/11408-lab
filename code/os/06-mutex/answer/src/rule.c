#include "rule.h"

#include <string.h>

/*
 * 四条都按王道第 2 章填，不按「只写破了哪一条」省略。
 * 单标志有限等待仍在：对方最多再进一次就把 turn 交回来。
 * 进不去是因为对方停在剩余区，这条算空闲让进失败。
 * 硬件指令和整型信号量没有等待队列，后来的进程可以一直抢先，有限等待失败。
 * 只有记录型在进不去时阻塞，四条都满足。唤醒按先来先到。
 */
static const char *k_lines[ALGO_COUNT] = {
    "algo=single progress=no mutex=yes bounded=yes yield=no",
    "algo=check-first progress=yes mutex=no bounded=no yield=no",
    "algo=check-after progress=no mutex=yes bounded=no yield=no",
    "algo=peterson progress=yes mutex=yes bounded=yes yield=no",
    "algo=tsl progress=yes mutex=yes bounded=no yield=no",
    "algo=swap progress=yes mutex=yes bounded=no yield=no",
    "algo=integer progress=yes mutex=yes bounded=no yield=no",
    "algo=record progress=yes mutex=yes bounded=yes yield=yes",
};

static const char *k_names[ALGO_COUNT] = {
    "single", "check-first", "check-after", "peterson", "tsl", "swap", "integer", "record",
};

int rule_id(const char *name) {
    int i;
    for (i = 0; i < ALGO_COUNT; i++) {
        if (strcmp(k_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *rule_name(int id) {
    if (id < 0 || id >= ALGO_COUNT) {
        return NULL;
    }
    return k_names[id];
}

const char *rule_line(int id) {
    if (id < 0 || id >= ALGO_COUNT) {
        return NULL;
    }
    return k_lines[id];
}

int rule_count(void) {
    return ALGO_COUNT;
}

const char *rule_at(int index) {
    return rule_line(index);
}

int rule_can_step(int id) {
    return id >= ALGO_SINGLE && id <= ALGO_SWAP;
}
