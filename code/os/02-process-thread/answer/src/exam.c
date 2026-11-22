#include "exam.h"

#include <string.h>

typedef struct {
    const char *name;
    int shared;
} ShareRow;

/*
 * 同一进程的线程共享地址空间里的代码、全局量、堆和已经打开的文件。
 * 栈、栈指针、程序计数器和寄存器是每个线程自己的执行现场。
 * 2011-25 的答案是栈指针；2024-28 的答案是地址空间和文件描述符可以共享，线程 T 的栈不行。
 */
static const ShareRow SHARE_ROWS[] = {
    {"code", 1},
    {"open-file", 1},
    {"global", 1},
    {"heap", 1},
    {"address-space", 1},
    {"fd", 1},
    {"stack", 0},
    {"stack-pointer", 0},
    {"register", 0},
    {"pc", 0},
};

typedef struct {
    const char *name;
    const char *truth;
} ClaimRow;

/*
 * 2019-23 的 B 把「每个用户级线程都有内核 TCB」说成普遍事实，只有一对一模型才是这样。
 * 2024-24 问终止进程时哪一步不一定做。回收内存、设备和 PCB 都要做；子进程可以变成孤儿，
 * 不必一起杀掉。王道终止原语写了「终止子孙」，和这道题的标准答案不一致，见 ANSWER.md。
 */
static const ClaimRow CLAIM_ROWS[] = {
    {"klt-os-schedule", "true"},
    {"ult-each-tcb", "false"},
    {"ult-switch-faster", "true"},
    {"ult-without-klt", "true"},
    {"kill-children", "false"},
    {"free-device", "true"},
    {"free-pcb", "true"},
    {"free-memory", "true"},
};

static const EventFact EVENT_ROWS[] = {
    {"p-wait", "running", "blocked", "block", "no", "yes"},
    {"mem-fail", "running", "blocked", "block", "no", "yes"},
    {"start-io", "running", "blocked", "block", "no", "yes"},
    {"preempt", "running", "ready", "preempt", "no", "no"},
    {"critical", "running", "blocked", "block", "no", "yes"},
    {"disk-read", "running", "blocked", "block", "no", "yes"},
    {"give-cpu", "running", "ready", "preempt", "no", "no"},
    {"io-end", "blocked", "ready", "wakeup", "yes", "no"},
    {"leave-cs", "blocked", "ready", "wakeup", "yes", "no"},
    {"timeslice", "running", "ready", "preempt", "no", "no"},
    {"keyboard", "running", "blocked", "block", "no", "yes"},
    {"page-fault", "running", "blocked", "block", "no", "yes"},
    {"yield", "running", "ready", "yield", "no", "no"},
    {"sem-wait", "running", "blocked", "block", "no", "yes"},
    {"admit", "new", "ready", "admit", "no", "no"},
    {"dispatch", "ready", "running", "dispatch", "no", "no"},
};

static const ModelFact MODEL_ROWS[] = {
    {"many-one", "n", "1", "process", "user", "no", "yes"},
    {"one-one", "n", "n", "thread", "kernel", "yes", "no"},
    {"many-many", "n", "m", "partial", "both", "yes", "no"},
};

static const EntityFact ENTITY_ROWS[] = {
    {"program", "yes", "no", "no"},
    {"process", "no", "yes", "no"},
    {"thread", "no", "no", "yes"},
};

typedef struct {
    const char *from;
    const char *to;
    const char *how;
} Arrow;

/*
 * 五态的箭头不变。七态再把就绪、阻塞按在不在内存拆开（考纲：进程的状态与转换）。
 * 挂起把活动进程换出外存，激活换回来。挂起阻塞等到的事件只解除等待，
 * 进程还在外存，所以进挂起就绪，不是活动就绪。正在运行的进程被挂起时并没有在等事件，
 * 进挂起就绪。创建时内存不够也可以直接落到挂起就绪，这条不叫 admit，因为没有进内存。
 * 撤销和五态一样，可以把挂起的 PCB 收走。
 */
static const Arrow LEGAL_ARROWS[] = {
    {"new", "ready", "admit"},
    {"new", "terminated", "revoke"},
    {"ready", "running", "dispatch"},
    {"ready", "terminated", "revoke"},
    {"running", "ready", "preempt-or-yield"},
    {"running", "blocked", "block"},
    {"running", "terminated", "exit-or-revoke"},
    {"blocked", "ready", "wakeup"},
    {"blocked", "terminated", "revoke"},
    {"ready", "suspend-ready", "suspend"},
    {"suspend-ready", "ready", "activate"},
    {"blocked", "suspend-blocked", "suspend"},
    {"suspend-blocked", "blocked", "activate"},
    {"suspend-blocked", "suspend-ready", "wakeup"},
    {"running", "suspend-ready", "suspend"},
    {"new", "suspend-ready", "no-mem"},
    {"suspend-ready", "terminated", "revoke"},
    {"suspend-blocked", "terminated", "revoke"},
};

static const char *FIELDS =
    "id=pid,uid cpu=pc,regs,psw,sp schedule=state,priority,event control=code,data,files,links";

static const char *STATE_NAMES[] = {
    "new", "ready", "running", "blocked", "terminated", "suspend-ready", "suspend-blocked",
};

bool exam_state_name(const char *name) {
    size_t i;
    if (name == NULL) {
        return false;
    }
    for (i = 0; i < sizeof STATE_NAMES / sizeof STATE_NAMES[0]; i++) {
        if (strcmp(STATE_NAMES[i], name) == 0) {
            return true;
        }
    }
    return false;
}

bool exam_share(const char *name, int *shared) {
    size_t i;
    if (name == NULL || shared == NULL) {
        return false;
    }
    for (i = 0; i < sizeof SHARE_ROWS / sizeof SHARE_ROWS[0]; i++) {
        if (strcmp(SHARE_ROWS[i].name, name) == 0) {
            *shared = SHARE_ROWS[i].shared;
            return true;
        }
    }
    return false;
}

bool exam_event(const char *name, const EventFact **fact) {
    size_t i;
    if (name == NULL || fact == NULL) {
        return false;
    }
    for (i = 0; i < sizeof EVENT_ROWS / sizeof EVENT_ROWS[0]; i++) {
        if (strcmp(EVENT_ROWS[i].name, name) == 0) {
            *fact = &EVENT_ROWS[i];
            return true;
        }
    }
    return false;
}

bool exam_model(const char *name, const ModelFact **fact) {
    size_t i;
    if (name == NULL || fact == NULL) {
        return false;
    }
    for (i = 0; i < sizeof MODEL_ROWS / sizeof MODEL_ROWS[0]; i++) {
        if (strcmp(MODEL_ROWS[i].name, name) == 0) {
            *fact = &MODEL_ROWS[i];
            return true;
        }
    }
    return false;
}

bool exam_entity(const char *name, const EntityFact **fact) {
    size_t i;
    if (name == NULL || fact == NULL) {
        return false;
    }
    for (i = 0; i < sizeof ENTITY_ROWS / sizeof ENTITY_ROWS[0]; i++) {
        if (strcmp(ENTITY_ROWS[i].name, name) == 0) {
            *fact = &ENTITY_ROWS[i];
            return true;
        }
    }
    return false;
}

bool exam_claim(const char *name, const char **truth) {
    size_t i;
    if (name == NULL || truth == NULL) {
        return false;
    }
    for (i = 0; i < sizeof CLAIM_ROWS / sizeof CLAIM_ROWS[0]; i++) {
        if (strcmp(CLAIM_ROWS[i].name, name) == 0) {
            *truth = CLAIM_ROWS[i].truth;
            return true;
        }
    }
    return false;
}

bool exam_trans(const char *from, const char *to, TransFact *fact) {
    size_t i;
    if (from == NULL || to == NULL || fact == NULL) {
        return false;
    }
    if (!exam_state_name(from) || !exam_state_name(to)) {
        return false;
    }
    fact->legal = 0;
    fact->how = "-";
    for (i = 0; i < sizeof LEGAL_ARROWS / sizeof LEGAL_ARROWS[0]; i++) {
        if (strcmp(LEGAL_ARROWS[i].from, from) == 0 && strcmp(LEGAL_ARROWS[i].to, to) == 0) {
            fact->legal = 1;
            fact->how = LEGAL_ARROWS[i].how;
            return true;
        }
    }
    return true;
}

const char *exam_fields(void) {
    return FIELDS;
}
