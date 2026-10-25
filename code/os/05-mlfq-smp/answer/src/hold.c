#include "hold.h"

#include <stdio.h>
#include <string.h>

static int known_region(const char *region) {
    return strcmp(region, "interrupt") == 0 || strcmp(region, "kernel-cs") == 0 ||
           strcmp(region, "user-cs") == 0 || strcmp(region, "atomic") == 0;
}

static int known_event(const char *event) {
    return strcmp(event, "exit") == 0 || strcmp(event, "block") == 0 ||
           strcmp(event, "slice") == 0 || strcmp(event, "higher") == 0 ||
           strcmp(event, "create") == 0 || strcmp(event, "return") == 0;
}

int hold_decide(const char *region, const char *event, Hold *out) {
    if (!known_region(region) || !known_event(event)) {
        return -1;
    }
    /* 题目前提是还有别的就绪进程，这些事件都「能」引起调度。 */
    out->want = 1;
    /*
     * return 本身就是离开内核、回到用户态的那一刻，2012-30 的 D 允许在这时调度。
     * 普通临界区访问的不是内核数据结构，慢速设备上如果不让切换，CPU 会空等。
     */
    if (strcmp(event, "return") == 0 || strcmp(region, "user-cs") == 0) {
        out->defer = 0;
    } else {
        out->defer = 1;
    }
    return 0;
}

void hold_print(const char *region, const char *event, const Hold *hold) {
    printf("region=%s event=%s want=%s switch=%s\n", region, event,
           hold->want ? "yes" : "no", hold->defer ? "defer" : "now");
    if (hold->defer) {
        printf("after leave switch=now\n");
    }
}
