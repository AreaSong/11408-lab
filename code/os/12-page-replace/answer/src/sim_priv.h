#ifndef SIM_PRIV_H
#define SIM_PRIV_H

#include "sim.h"

int queue_sim(int lru, int frames, const Access *warm, int nwarm, const Access *refs,
              int nrefs, Report *out);
int opt_sim(int frames, const Access *warm, int nwarm, const Access *refs, int nrefs,
            Report *out);
int clock_sim(int improved, int frames, const Access *warm, int nwarm, const Access *refs,
              int nrefs, Report *out);

#endif
