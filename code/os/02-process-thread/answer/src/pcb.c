#include "pcb.h"

#include <stdio.h>
#include <string.h>

static const char *state_name(PcbState state) {
    switch (state) {
    case PCB_NEW:
        return "new";
    case PCB_READY:
        return "ready";
    case PCB_RUNNING:
        return "running";
    case PCB_BLOCKED:
        return "blocked";
    case PCB_TERMINATED:
        return "terminated";
    default:
        return "-";
    }
}

void pcb_init(ProcTable *table) {
    int i;
    memset(table, 0, sizeof *table);
    table->ready_head = -1;
    table->ready_tail = -1;
    table->running = -1;
    for (i = 0; i < PCB_REASONS; i++) {
        table->block_head[i] = -1;
        table->block_tail[i] = -1;
    }
    for (i = 0; i < PCB_MAX; i++) {
        table->slot[i].link = -1;
    }
}

static int find_pid(const ProcTable *table, const char *pid) {
    int i;
    for (i = 0; i < PCB_MAX; i++) {
        if (table->slot[i].state != PCB_EMPTY && strcmp(table->slot[i].pid, pid) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_slot(ProcTable *table) {
    int i;
    for (i = 0; i < PCB_MAX; i++) {
        if (table->slot[i].state == PCB_EMPTY) {
            return i;
        }
    }
    return -1;
}

static void queue_push(ProcTable *table, int *head, int *tail, int idx) {
    table->slot[idx].link = -1;
    if (*head < 0) {
        *head = idx;
        *tail = idx;
        return;
    }
    table->slot[*tail].link = idx;
    *tail = idx;
}

static void queue_unlink(ProcTable *table, int *head, int *tail, int idx) {
    int prev = -1;
    int cur = *head;
    while (cur >= 0) {
        int next = table->slot[cur].link;
        if (cur == idx) {
            if (prev < 0) {
                *head = next;
            } else {
                table->slot[prev].link = next;
            }
            if (*tail == idx) {
                *tail = prev;
            }
            table->slot[idx].link = -1;
            return;
        }
        prev = cur;
        cur = next;
    }
}

static int reason_slot(ProcTable *table, const char *reason, int create) {
    int i;
    for (i = 0; i < table->nblock; i++) {
        if (strcmp(table->block_name[i], reason) == 0) {
            return i;
        }
    }
    if (!create || table->nblock >= PCB_REASONS) {
        return -1;
    }
    snprintf(table->block_name[table->nblock], PCB_REASON, "%s", reason);
    table->block_head[table->nblock] = -1;
    table->block_tail[table->nblock] = -1;
    return table->nblock++;
}

static void append_piece(char *buf, size_t n, int *used, const char *name, int with_sep, char sep) {
    int wrote;
    size_t room;
    if ((size_t)*used >= n) {
        return;
    }
    room = n - (size_t)*used;
    if (with_sep) {
        wrote = snprintf(buf + *used, room, "%c%s", sep, name);
    } else {
        wrote = snprintf(buf + *used, room, "%s", name);
    }
    if (wrote > 0 && (size_t)wrote < room) {
        *used += wrote;
    }
}

static void format_chain(const ProcTable *table, int head, char *buf, size_t n) {
    int cur = head;
    int used = 0;
    buf[0] = '\0';
    while (cur >= 0) {
        append_piece(buf, n, &used, table->slot[cur].pid, used > 0, '+');
        cur = table->slot[cur].link;
    }
}

static void format_ready(const ProcTable *table, char *buf, size_t n) {
    if (table->ready_head < 0) {
        snprintf(buf, n, "-");
        return;
    }
    format_chain(table, table->ready_head, buf, n);
}

/* 空队列不打印。同一个原因里的进程用 +，不同原因之间用逗号，原因按第一次阻塞的顺序。 */
static void format_blocked(const ProcTable *table, char *buf, size_t n) {
    int i;
    int used = 0;
    buf[0] = '\0';
    for (i = 0; i < table->nblock; i++) {
        char names[PCB_QTEXT];
        char piece[PCB_QTEXT];
        if (table->block_head[i] < 0) {
            continue;
        }
        format_chain(table, table->block_head[i], names, sizeof names);
        snprintf(piece, sizeof piece, "%s:%s", table->block_name[i], names);
        append_piece(buf, n, &used, piece, used > 0, ',');
    }
    if (used == 0) {
        snprintf(buf, n, "-");
    }
}

static void fill_trace(const ProcTable *table, PcbTrace *out, int step, const char *event,
                       const char *pid, const char *from, const char *to, int ok, const char *why,
                       const char *via) {
    out->step = step;
    snprintf(out->pid, sizeof out->pid, "%s", pid);
    snprintf(out->event, sizeof out->event, "%s", event);
    snprintf(out->from, sizeof out->from, "%s", from);
    snprintf(out->to, sizeof out->to, "%s", to);
    out->ok = ok;
    snprintf(out->why, sizeof out->why, "%s", why);
    snprintf(out->via, sizeof out->via, "%s", via);
    format_ready(table, out->ready, sizeof out->ready);
    format_blocked(table, out->blocked, sizeof out->blocked);
    if (table->running >= 0) {
        snprintf(out->running, sizeof out->running, "%s", table->slot[table->running].pid);
    } else {
        snprintf(out->running, sizeof out->running, "-");
    }
}

static void reject(const ProcTable *table, PcbTrace *out, int step, const char *event,
                   const char *pid, int idx, const char *to, const char *why) {
    const char *from = "-";
    if (idx >= 0) {
        from = state_name(table->slot[idx].state);
    }
    fill_trace(table, out, step, event, pid, from, to, 0, why, "-");
}

static void do_create(ProcTable *table, int step, const char *pid, PcbTrace *out) {
    int idx = find_pid(table, pid);
    int slot;
    if (idx >= 0) {
        reject(table, out, step, "create", pid, idx, "ready", "exists");
        return;
    }
    slot = alloc_slot(table);
    if (slot < 0) {
        reject(table, out, step, "create", pid, -1, "ready", "full");
        return;
    }
    /* 资源够：创建态只是过路，原语结束时进程已经在就绪队列。via=new 把这一步记下来。 */
    snprintf(table->slot[slot].pid, PCB_NAME, "%s", pid);
    table->slot[slot].state = PCB_READY;
    table->slot[slot].reason[0] = '\0';
    queue_push(table, &table->ready_head, &table->ready_tail, slot);
    fill_trace(table, out, step, "create", pid, "-", "ready", 1, "ok", "new");
}

static void do_dispatch(ProcTable *table, int step, const char *pid, PcbTrace *out) {
    int idx = find_pid(table, pid);
    if (idx < 0) {
        reject(table, out, step, "dispatch", pid, -1, "running", "missing");
        return;
    }
    if (table->slot[idx].state != PCB_READY) {
        reject(table, out, step, "dispatch", pid, idx, "running", "not-ready");
        return;
    }
    if (table->running >= 0) {
        reject(table, out, step, "dispatch", pid, idx, "running", "cpu-busy");
        return;
    }
    queue_unlink(table, &table->ready_head, &table->ready_tail, idx);
    table->slot[idx].state = PCB_RUNNING;
    table->running = idx;
    fill_trace(table, out, step, "dispatch", pid, "ready", "running", 1, "ok", "-");
}

/* 运行态让出 CPU，进程仍可运行，所以回就绪队列尾，不进阻塞队列。 */
static void do_to_ready(ProcTable *table, int step, const char *event, const char *pid,
                        PcbTrace *out) {
    int idx = find_pid(table, pid);
    if (idx < 0) {
        reject(table, out, step, event, pid, -1, "ready", "missing");
        return;
    }
    if (table->running != idx) {
        reject(table, out, step, event, pid, idx, "ready", "not-running");
        return;
    }
    table->slot[idx].state = PCB_READY;
    table->running = -1;
    queue_push(table, &table->ready_head, &table->ready_tail, idx);
    fill_trace(table, out, step, event, pid, "running", "ready", 1, "ok", "-");
}

static void do_block(ProcTable *table, int step, const char *pid, const char *reason, PcbTrace *out) {
    int idx = find_pid(table, pid);
    int reason_at;
    if (idx < 0) {
        reject(table, out, step, "block", pid, -1, "blocked", "missing");
        return;
    }
    if (table->running != idx) {
        reject(table, out, step, "block", pid, idx, "blocked", "not-running");
        return;
    }
    reason_at = reason_slot(table, reason, 1);
    if (reason_at < 0) {
        reject(table, out, step, "block", pid, idx, "blocked", "full");
        return;
    }
    table->slot[idx].state = PCB_BLOCKED;
    snprintf(table->slot[idx].reason, PCB_REASON, "%s", reason);
    table->running = -1;
    queue_push(table, &table->block_head[reason_at], &table->block_tail[reason_at], idx);
    fill_trace(table, out, step, "block", pid, "running", "blocked", 1, "ok", "-");
}

static void do_wakeup(ProcTable *table, int step, const char *pid, PcbTrace *out) {
    int idx = find_pid(table, pid);
    int reason_at;
    if (idx < 0) {
        reject(table, out, step, "wakeup", pid, -1, "ready", "missing");
        return;
    }
    if (table->slot[idx].state != PCB_BLOCKED) {
        reject(table, out, step, "wakeup", pid, idx, "ready", "not-blocked");
        return;
    }
    reason_at = reason_slot(table, table->slot[idx].reason, 0);
    if (reason_at >= 0) {
        queue_unlink(table, &table->block_head[reason_at], &table->block_tail[reason_at], idx);
    }
    table->slot[idx].state = PCB_READY;
    table->slot[idx].reason[0] = '\0';
    queue_push(table, &table->ready_head, &table->ready_tail, idx);
    fill_trace(table, out, step, "wakeup", pid, "blocked", "ready", 1, "ok", "-");
}

static void recycle(ProcTable *table, int idx) {
    PcbState state = table->slot[idx].state;
    if (state == PCB_READY) {
        queue_unlink(table, &table->ready_head, &table->ready_tail, idx);
    } else if (state == PCB_RUNNING) {
        table->running = -1;
    } else if (state == PCB_BLOCKED) {
        int reason_at = reason_slot(table, table->slot[idx].reason, 0);
        if (reason_at >= 0) {
            queue_unlink(table, &table->block_head[reason_at], &table->block_tail[reason_at], idx);
        }
    }
    memset(&table->slot[idx], 0, sizeof table->slot[idx]);
    table->slot[idx].link = -1;
}

static int can_revoke(PcbState state) {
    return state == PCB_READY || state == PCB_RUNNING || state == PCB_BLOCKED || state == PCB_NEW;
}

static void do_revoke(ProcTable *table, int step, const char *pid, PcbTrace *out) {
    int idx = find_pid(table, pid);
    const char *from;
    if (idx < 0) {
        reject(table, out, step, "revoke", pid, -1, "terminated", "missing");
        return;
    }
    if (!can_revoke(table->slot[idx].state)) {
        reject(table, out, step, "revoke", pid, idx, "terminated", "bad-state");
        return;
    }
    from = state_name(table->slot[idx].state);
    recycle(table, idx);
    fill_trace(table, out, step, "revoke", pid, from, "terminated", 1, "ok", "-");
}

static void do_exit(ProcTable *table, int step, const char *pid, PcbTrace *out) {
    int idx = find_pid(table, pid);
    if (idx < 0) {
        reject(table, out, step, "exit", pid, -1, "terminated", "missing");
        return;
    }
    if (table->running != idx) {
        reject(table, out, step, "exit", pid, idx, "terminated", "not-running");
        return;
    }
    recycle(table, idx);
    fill_trace(table, out, step, "exit", pid, "running", "terminated", 1, "ok", "-");
}

static int known_event(const char *event) {
    return strcmp(event, "create") == 0 || strcmp(event, "dispatch") == 0 ||
           strcmp(event, "preempt") == 0 || strcmp(event, "yield") == 0 ||
           strcmp(event, "block") == 0 || strcmp(event, "wakeup") == 0 ||
           strcmp(event, "revoke") == 0 || strcmp(event, "exit") == 0;
}

bool pcb_apply(ProcTable *table, int step, const char *event, const char *pid, const char *extra,
               PcbTrace *out) {
    if (table == NULL || event == NULL || pid == NULL || out == NULL || !known_event(event)) {
        return false;
    }
    if (strcmp(event, "block") == 0) {
        if (extra == NULL || extra[0] == '\0') {
            return false;
        }
        do_block(table, step, pid, extra, out);
        return true;
    }
    if (extra != NULL) {
        return false;
    }
    if (strcmp(event, "create") == 0) {
        do_create(table, step, pid, out);
    } else if (strcmp(event, "dispatch") == 0) {
        do_dispatch(table, step, pid, out);
    } else if (strcmp(event, "preempt") == 0 || strcmp(event, "yield") == 0) {
        do_to_ready(table, step, event, pid, out);
    } else if (strcmp(event, "wakeup") == 0) {
        do_wakeup(table, step, pid, out);
    } else if (strcmp(event, "revoke") == 0) {
        do_revoke(table, step, pid, out);
    } else {
        do_exit(table, step, pid, out);
    }
    return true;
}

void pcb_format(const PcbTrace *trace, char *buf, size_t n) {
    snprintf(buf, n,
             "%d pid=%s event=%s from=%s to=%s ok=%s why=%s via=%s ready=%s blocked=%s running=%s",
             trace->step, trace->pid, trace->event, trace->from, trace->to, trace->ok ? "yes" : "no",
             trace->why, trace->via, trace->ready, trace->blocked, trace->running);
}
