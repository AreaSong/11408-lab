#ifndef POLICY_H
#define POLICY_H

#include "sched.h"

int sched_name_ok(const char *name);
const char *policy_name(SchedAlgo algo);
int policy_index(const char *name, SchedAlgo *out);
int policy_uses_queue(SchedAlgo algo);
const char *policy_kind(SchedAlgo algo);

/* a 比 b 更该被选中时返回 1。平局：先到达，再比输入下标。 */
int policy_better(const SchedJob *jobs, const long long *remain, long long now, PrioOrder order,
                  SchedAlgo algo, int a, int b);

/* 只有严格更短的剩余时间、或严格更高的优先级，才抢走正在运行的进程。 */
int policy_preempt(const SchedJob *jobs, const long long *remain, PrioOrder order, SchedAlgo algo,
                   int challenger, int current);

#endif
