#ifndef PREP_H
#define PREP_H

#include "sched.h"

int sched_prepare(SchedResult *out, SchedAlgo algo, long long quantum, long long switch_cost,
                  PrioOrder order, const SchedJob *jobs, int n);

#endif
