#ifndef WSET_H
#define WSET_H

#include "sim.h"

int work_set(const int *pages, int n, int window, int *uniq, int *nu);
void show_report(const Report *r);
void show_sweep_frames(const char *algo, const Access *refs, int n);
void show_sweep_algos(int frames, const Access *refs, int n);
void show_locality(int frames);
void show_size(const char *algo, int frames, int page_bytes, const int *addr, int n);
void show_thrash(const char *algo, const Access *refs, int n);
void show_belady(void);
void show_work(int window, const int *pages, int n);

#endif
