#include "prompt.h"
#include "threads.h"

#include <stdio.h>
#include <string.h>

static void live_pc(void) {
    ThreadPc r;
    if (thread_pc(&r) != 0) {
        printf("error\n");
        return;
    }
    printf("pc produced=%d consumed=%d sum=%d overflow=%s\n", r.produced, r.consumed, r.sum,
           r.overflow ? "yes" : "no");
}

static void live_rw(void) {
    ThreadRw r;
    if (thread_rw(&r) != 0) {
        printf("error\n");
        return;
    }
    printf("rw reads=%d writes=%d overlap=%s\n", r.reads, r.writes, r.overlap ? "yes" : "no");
}

static void live_phi(int scheme, const char *name) {
    ThreadPhi r;
    if (thread_phi(scheme, &r) != 0) {
        printf("error\n");
        return;
    }
    printf("phi scheme=%s eaten=%d bad=%s\n", name, r.eaten, r.bad ? "yes" : "no");
}

static void live_smoke(void) {
    ThreadSmoke r;
    if (thread_smoke(&r) != 0) {
        printf("error\n");
        return;
    }
    printf("smoke smoked=%d glue=%d paper=%d tobacco=%d\n", r.smoked, r.glue, r.paper, r.tobacco);
}

static void live_plant(void) {
    ThreadPlant r;
    if (thread_plant(&r) != 0) {
        printf("error\n");
        return;
    }
    printf("plant watered=%d within=%s bad=%s\n", r.watered,
           r.max_open >= 1 && r.max_open <= 3 ? "yes" : "no", r.bad ? "yes" : "no");
}

static void live_bowl(void) {
    ThreadBowl r;
    if (thread_bowl(&r) != 0) {
        printf("error\n");
        return;
    }
    printf("bowl eaten=%d within=%s bad=%s\n", r.eaten,
           r.max_users >= 1 && r.max_users <= 2 ? "yes" : "no", r.bad ? "yes" : "no");
}

static void live_wrong(void) {
    ThreadWrong r;
    if (thread_wrong(&r) != 0) {
        printf("finished=timeout\n");
        return;
    }
    printf("finished=no empty_waiters=%d mutex_waiters=%d\n", r.empty_waiters, r.mutex_waiters);
}

static int one(const char *cmd) {
    if (strcmp(cmd, "pc") == 0) {
        live_pc();
    } else if (strcmp(cmd, "rw") == 0) {
        live_rw();
    } else if (strcmp(cmd, "limit") == 0) {
        live_phi(THREAD_PHI_LIMIT, "limit");
    } else if (strcmp(cmd, "odd") == 0) {
        live_phi(THREAD_PHI_ODD, "odd");
    } else if (strcmp(cmd, "smoke") == 0) {
        live_smoke();
    } else if (strcmp(cmd, "plant") == 0) {
        live_plant();
    } else if (strcmp(cmd, "bowl") == 0) {
        live_bowl();
    } else if (strcmp(cmd, "wrong") == 0) {
        live_wrong();
    } else {
        printf("bad-command\n");
        return 1;
    }
    return 0;
}

int main(void) {
    char line[64];
    for (;;) {
        size_t n;
        prompt("%s", "请输入命令，Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;
        }
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (line[0] == '\0') {
            continue;
        }
        if (one(line) != 0) {
            return 1;
        }
    }
}
