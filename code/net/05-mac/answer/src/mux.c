#include "mux.h"

int mux_range(int users, int busy, int data, int addr) {
    return users >= 1 && users <= 32 && busy >= 0 && busy <= users && data >= 1 && data <= 4096 && addr >= 1 &&
           addr <= 64;
}

void mux_tdm(int users, int busy, int data, MuxUse *out) {
    out->useful = (long long)busy * data;
    out->total = (long long)users * data;
}

void mux_stdm(int busy, int data, int addr, MuxUse *out) {
    out->useful = (long long)busy * data;
    out->total = (long long)busy * (data + addr);
}

int mux_cmp(const MuxUse *tdm, const MuxUse *stdm) {
    long long left;
    long long right;
    if (tdm->total <= 0 || stdm->total <= 0) {
        return 0;
    }
    left = stdm->useful * tdm->total;
    right = tdm->useful * stdm->total;
    if (left > right) {
        return 1;
    }
    if (left < right) {
        return -1;
    }
    return 0;
}
