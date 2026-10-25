#include "space.h"

#include <string.h>

int space_jobs(const Space *s, Job *out, int cap) {
    int copy = s->nj < cap ? s->nj : cap;
    if (copy > 0) {
        memcpy(out, s->jobs, (size_t)copy * sizeof *out);
    }
    return s->nj;
}

void space_stat(const Space *s, long long *sum, long long *largest) {
    int i;
    *sum = 0;
    *largest = 0;
    for (i = 0; i < s->nh; i++) {
        *sum += s->holes[i].size;
        if (s->holes[i].size > *largest) {
            *largest = s->holes[i].size;
        }
    }
}

int space_cursor(const Space *s, long long *out) {
    if (s->algo != ALGO_NF || !s->cursor_valid) {
        return -1;
    }
    *out = s->cursor;
    return 0;
}
