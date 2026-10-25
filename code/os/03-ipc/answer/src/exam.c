#include "exam.h"

#include <string.h>

/*
 * kernel=setup：内核只在建立共享区时介入，之后这次通信的读写留在用户态。
 * kernel=every：每一次传递都要进内核拷贝或投递。
 * sync=user：共享区不管顺序，双方要自己用同步工具。
 * sync=os：空、满或 send/receive 由内核把进程堵上。
 * sync=async：信号是通知，不负责给一批数据排先后。
 * kin=either：匿名管道要亲缘关系，命名管道不要。模式这一行不把两者写成同一个 yes。
 */
static const ModeFact k_modes[] = {
    {"shared", "full", "setup", "user", "region", "yes", "no", "memory"},
    {"message", "full", "every", "os", "message", "yes", "no", "memory"},
    {"pipe", "half", "every", "os", "byte", "yes", "either", "memory"},
    {"signal", "notify", "every", "async", "number", "no", "no", "none"},
};

/* 王道把共享存储分成低级的共享数据结构、高级的共享存储区；消息传递分成直接和信箱。 */
static const KindFact k_kinds[] = {
    {"direct", "target=pid mailbox=no"},
    {"mailbox", "target=box mailbox=yes"},
    {"anon", "kin=yes named=no"},
    {"fifo", "kin=no named=yes"},
    {"region", "bulk=yes level=high"},
    {"struct", "bulk=no level=low"},
};

static const struct {
    const char *name;
    const char *truth;
} k_claims[] = {
    {"pipe-bidir", "false"},
    {"pipe-disk", "false"},
    {"pipe-rw-block", "true"},
    {"pipe-one-end", "false"},
    {"shared-user-sync", "true"},
    {"message-boundary", "true"},
    {"pipe-boundary", "false"},
    {"signal-bulk", "false"},
    {"sigkill-catch", "false"},
    {"sigstop-catch", "false"},
    {"sigterm-catch", "true"},
    {"mmap-ipc", "true"},
};

static const ModeFact *find_mode(const char *name) {
    size_t i;
    for (i = 0; i < sizeof k_modes / sizeof k_modes[0]; i++) {
        if (strcmp(k_modes[i].name, name) == 0) {
            return &k_modes[i];
        }
    }
    return NULL;
}

bool exam_mode(const char *name, const ModeFact **fact) {
    const ModeFact *found = find_mode(name);
    if (found == NULL) {
        return false;
    }
    *fact = found;
    return true;
}

bool exam_kind(const char *name, const KindFact **fact) {
    size_t i;
    for (i = 0; i < sizeof k_kinds / sizeof k_kinds[0]; i++) {
        if (strcmp(k_kinds[i].name, name) == 0) {
            *fact = &k_kinds[i];
            return true;
        }
    }
    return false;
}

bool exam_claim(const char *name, const char **truth) {
    size_t i;
    for (i = 0; i < sizeof k_claims / sizeof k_claims[0]; i++) {
        if (strcmp(k_claims[i].name, name) == 0) {
            *truth = k_claims[i].truth;
            return true;
        }
    }
    return false;
}

const ModeFact *exam_card(int index) {
    if (index < 0 || (size_t)index >= sizeof k_modes / sizeof k_modes[0]) {
        return NULL;
    }
    return &k_modes[index];
}
