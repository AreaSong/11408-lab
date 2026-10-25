#include "claim.h"

#include <string.h>

typedef struct {
    const char *name;
    const char *truth;
} ClaimRow;

/*
 * 2023-23 问的是微内核「具有」哪些特征。较好的性能是宏内核的，不是微内核的。
 * 可靠性、安全性、可扩展性来自：服务放在用户态，坏一个服务不拖垮内核，加服务不用改内核。
 * 2025-24 问哪句是错的。VMM 必须比客户操作系统特权更高，才能截住特权指令。
 * 「一台主机模拟多种指令集」这句题目算对，它其实靠近仿真，但标准答案不把它当错句。
 */
static const ClaimRow CLAIM_ROWS[] = {
    {"micro-performance", "false"},
    {"micro-reliability", "true"},
    {"micro-security", "true"},
    {"micro-extensible", "true"},
    {"os-on-vm", "true"},
    {"many-vm", "true"},
    {"vmm-same-privilege", "false"},
    {"multi-isa", "true"},
};

bool claim_lookup(const char *name, const char **true_false) {
    size_t n = sizeof CLAIM_ROWS / sizeof CLAIM_ROWS[0];
    size_t i;
    if (name == NULL || true_false == NULL) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(CLAIM_ROWS[i].name, name) == 0) {
            *true_false = CLAIM_ROWS[i].truth;
            return true;
        }
    }
    return false;
}
