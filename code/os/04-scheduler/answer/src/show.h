#ifndef SHOW_H
#define SHOW_H

#include "frac.h"
#include "sched.h"

int show_metrics(const SchedResult *r, Frac *turn, Frac *weight, Frac *wait);
void show_run(const SchedResult *r);
void show_pick(const SchedResult *r);
void show_avg_line(const SchedResult *r);

#endif
