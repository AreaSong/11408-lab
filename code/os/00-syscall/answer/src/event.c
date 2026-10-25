#include "event.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *klass;
    const char *subtype;
    const char *ret_to;
} EventRow;

/* return：next 下一条，current 当前指令，none 不返回。 */
static const EventRow EVENT_ROWS[] = {
    /* 外中断：来自 CPU 外面，与当前指令无关，异步，返回下一条 */
    {"keyboard", "interrupt", "external", "next"},
    {"clock", "interrupt", "external", "next"},
    {"io-done", "interrupt", "external", "next"},
    {"dma-done", "interrupt", "external", "next"},
    /* 故障：同步，处理后重新执行惹出故障的那条指令 */
    {"page-fault", "exception", "fault", "current"},
    {"float-underflow", "exception", "fault", "current"},
    /* 终止：同步，现场已经不可恢复 */
    {"div-zero", "exception", "abort", "none"},
    {"illegal", "exception", "abort", "none"},
    {"protection", "exception", "abort", "none"},
    /* 自陷：程序有意安排的陷入，服务完执行下一条 */
    {"syscall", "exception", "trap", "next"},
    {"breakpoint", "exception", "trap", "next"},
};

bool event_lookup(const char *name, const char **klass, const char **subtype,
                  const char **ret_to) {
    if (name == NULL || klass == NULL || subtype == NULL || ret_to == NULL) {
        return false;
    }
    size_t n = sizeof EVENT_ROWS / sizeof EVENT_ROWS[0];
    for (size_t i = 0; i < n; i++) {
        if (strcmp(EVENT_ROWS[i].name, name) != 0) {
            continue;
        }
        *klass = EVENT_ROWS[i].klass;
        *subtype = EVENT_ROWS[i].subtype;
        *ret_to = EVENT_ROWS[i].ret_to;
        return true;
    }
    return false;
}
