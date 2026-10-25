#ifndef HOLD_H
#define HOLD_H

/*
 * 某个调度事件发生时，CPU 正处在哪一类区域。
 * 王道：中断处理、内核程序临界区、原语里不能切换；
 * 普通临界区可以切换。系统调用或中断返回用户态时可以调度。
 */
typedef struct {
    int want;
    int defer;
} Hold;

int hold_decide(const char *region, const char *event, Hold *out);
void hold_print(const char *region, const char *event, const Hold *hold);

#endif
