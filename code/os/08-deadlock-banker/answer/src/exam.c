#include "exam.h"

#include <string.h>

/*
 * 卡片按王道第 2 章的用词。预防是事先破坏一个必要条件，避免是银行家算法守住安全状态，
 * 检测看的是当前申请，不看 Max。claim 把 2015-26、2019-30 的句子拆开。
 */
static const char *k_rows[] = {
    "cond mutex share=no break=hard",
    "cond hold name=request-and-hold break=allocate-all",
    "cond nopreempt release=voluntary break=preempt",
    "cond cycle chain=wait-for break=ordered-request",
    "strategy prevent when=before ensure=yes info=none",
    "strategy avoid when=on-request banker=yes info=max",
    "strategy detect when=after graph=yes info=no-max then=recover",
    "relation safe implies=no-deadlock",
    "relation unsafe implies=maybe-deadlock",
    "relation deadlock means=all-blocked-in-set",
    "relation starve means=one-waits others-run",
    "rule tie safety=smallest-index",
    "rule tie recover=min-cost then-smallest",
    "rule need formula=max-alloc",
    "rule request checks=need,avail,safe",
    "rule cycle one-instance=deadlock-iff",
    "rule cycle many-instance=necessary-only",
    "rule least formula=sum(k-1)+1",
};

static const struct {
    const char *name;
    const char *truth;
} k_claims[] = {
    {"s1-limit-order", "false"},
    {"s1-need-max", "true"},
    {"s1-refuse-unsafe", "true"},
    {"deprive-recover", "true"},
    {"prevent-ensures", "true"},
    {"banker-detects", "false"},
    {"deadlock-two-block", "true"},
    {"unsafe-is-dead", "false"},
    {"safe-no-dead", "true"},
    {"banker-avoid", "true"},
    {"banker-break-hold", "false"},
    {"order-break-cycle", "true"},
    {"all-break-hold", "true"},
    {"preempt-break-nopreempt", "true"},
    {"break-mutex-hard", "true"},
    {"cycle-one-iff", "true"},
    {"cycle-many-enough", "false"},
    {"starve-is-dead", "false"},
};

int exam_card_count(void) {
    return (int)(sizeof k_rows / sizeof k_rows[0]);
}

const char *exam_card(int index) {
    if (index < 0 || index >= exam_card_count()) {
        return NULL;
    }
    return k_rows[index];
}

const char *exam_claim(const char *name) {
    int i;
    int n = (int)(sizeof k_claims / sizeof k_claims[0]);
    for (i = 0; i < n; i++) {
        if (strcmp(k_claims[i].name, name) == 0) {
            return k_claims[i].truth;
        }
    }
    return NULL;
}
