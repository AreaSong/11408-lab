/*
 * 进程状态转换。每行一个事件，trace 按「谁、从哪到哪、队列变成什么样」打印。
 * ok=no 时进程留在原队列，to 是这次想去但没去成的状态。
 */
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "pcb.h"

static int apply_line(ProcTable *table, int *step, const char *event, const char *pid,
                      const char *extra) {
    PcbTrace trace;
    char text[512];
    if (!pcb_apply(table, *step + 1, event, pid, extra, &trace)) {
        printf("%s unknown\n", event);
        return 0;
    }
    *step += 1;
    pcb_format(&trace, text, sizeof text);
    printf("%s\n", text);
    return 0;
}

static int handle_line(ProcTable *table, int *step, char *line) {
    char event[16];
    char pid[PCB_NAME];
    char extra[PCB_REASON];
    int got = sscanf(line, "%15s %15s %15s", event, pid, extra);
    if (got < 1) {
        printf("bad-command\n");
        return 1;
    }
    if (strcmp(event, "block") == 0) {
        if (got != 3) {
            printf("missing-arg\n");
            return 1;
        }
        return apply_line(table, step, event, pid, extra);
    }
    if (got != 2) {
        printf(got < 2 ? "missing-arg\n" : "bad-command\n");
        return 1;
    }
    return apply_line(table, step, event, pid, NULL);
}

int main(void) {
    ProcTable table;
    char line[160];
    int step = 0;
    int failed = 0;
    pcb_init(&table);
    while (cli_read(line, sizeof line, "请输入进程事件（命令 进程 参数），Ctrl+D 结束：")) {
        failed |= handle_line(&table, &step, line);
    }
    return failed ? 1 : 0;
}
