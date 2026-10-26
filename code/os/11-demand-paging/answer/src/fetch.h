#ifndef FETCH_H
#define FETCH_H

#define FETCH_FRAMES 32

int fetch_run(int frames, int ahead, int space, const int *refs, int nref, int *faults, int *prefetched);

#endif
