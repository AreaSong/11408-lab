#include "claim.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *truth;
} ClaimRow;

/*
 * 2019-25：服务程序在内核态；系统调用挡住用户程序直接碰外设；
 * 各操作系统的系统调用接口并不统一；系统调用就是内核给应用程序的接口。
 * 2024-23：「中断或异常发生时 CPU 处于内核态」说死了，不成立——
 * 发生的那一瞬间多半还在用户态，中断嵌套时才已经在内核态。
 * 处理程序开始执行时，硬件已经完成态切换，那时一定在内核态。
 */
static const ClaimRow CLAIM_ROWS[] = {
    {"service-in-kernel", "true"},
    {"syscall-hides-device", "true"},
    {"unified-abi", "false"},
    {"syscall-is-interface", "true"},
    {"event-time-always-kernel", "false"},
    {"each-syscall-has-routine", "true"},
    {"handler-starts-in-kernel", "true"},
    {"new-device-registers-isr", "true"},
};

bool claim_lookup(const char *name, const char **true_false) {
    if (name == NULL || true_false == NULL) {
        return false;
    }
    size_t n = sizeof CLAIM_ROWS / sizeof CLAIM_ROWS[0];
    for (size_t i = 0; i < n; i++) {
        if (strcmp(CLAIM_ROWS[i].name, name) == 0) {
            *true_false = CLAIM_ROWS[i].truth;
            return true;
        }
    }
    return false;
}
