#ifndef PCB_H
#define PCB_H

#include <stdbool.h>
#include <stddef.h>

#define PCB_NAME 16
#define PCB_REASON 16
#define PCB_MAX 16
#define PCB_REASONS 8
#define PCB_QTEXT 160

typedef enum {
    PCB_EMPTY = 0,
    PCB_NEW,
    PCB_READY,
    PCB_RUNNING,
    PCB_BLOCKED,
    PCB_TERMINATED
} PcbState;

/* 进程控制块。link 把同一队列里的 PCB 串起来，下标就是链接，-1 表示链尾。 */
typedef struct {
    char pid[PCB_NAME];
    PcbState state;
    char reason[PCB_REASON];
    int link;
} Pcb;

/*
 * 单处理机：running 同时最多一个。
 * 就绪一条链；阻塞按等待原因各一条链。原因的顺序是第一次出现的顺序。
 */
typedef struct {
    Pcb slot[PCB_MAX];
    int ready_head;
    int ready_tail;
    int block_head[PCB_REASONS];
    int block_tail[PCB_REASONS];
    char block_name[PCB_REASONS][PCB_REASON];
    int nblock;
    int running;
} ProcTable;

typedef struct {
    int step;
    char pid[PCB_NAME];
    char event[16];
    char from[16];
    char to[16];
    int ok;
    char why[16];
    char via[8];
    char ready[PCB_QTEXT];
    char blocked[PCB_QTEXT];
    char running[PCB_NAME];
} PcbTrace;

void pcb_init(ProcTable *table);

/* 事件认识就返回 true，哪怕这次转换不合法。事件不认识返回 false。 */
bool pcb_apply(ProcTable *table, int step, const char *event, const char *pid,
               const char *extra, PcbTrace *out);

void pcb_format(const PcbTrace *trace, char *buf, size_t n);

#endif
