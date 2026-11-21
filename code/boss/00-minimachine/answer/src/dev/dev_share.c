#include "dev/dev_share.h"

#include "dev/dev_fmt.h"

static int fill_share(double hz, double times, double cycles, double gap_s, int keeps, DevShare *out) {
    if (out == NULL || !(hz > 0) || !(times > 0) || !(cycles > 0)) {
        return -1;
    }
    out->times = times;
    out->cycles = cycles;
    out->ratio = times * cycles / hz;
    out->keeps = keeps;
    dev_fmt_pct(out->pct, sizeof out->pct, out->ratio);
    if (gap_s > 0) {
        dev_fmt_time(out->gap, sizeof out->gap, gap_s);
    } else {
        out->gap[0] = '\0';
    }
    return 0;
}

int dev_share_poll(double hz, double cycles, double times, DevShare *out) {
    return fill_share(hz, times, cycles, 0, 1, out);
}

int dev_share_query(double hz, double cpi, double insns, double bps, double bytes, DevShare *out) {
    double times;
    double gap;
    if (!(cpi > 0) || !(insns > 0) || !(bps > 0) || !(bytes > 0)) {
        return -1;
    }
    times = bps / bytes;
    gap = bytes / bps;
    return fill_share(hz, times, insns * cpi, gap, gap + 1e-15 >= insns * cpi / hz, out);
}

int dev_share_intr(double hz, double cpi, double service, double extra, double bps, double bytes,
                   DevShare *out) {
    if (!(cpi > 0) || service < 0 || extra < 0 || !(bps > 0) || !(bytes > 0)) {
        return -1;
    }
    return fill_share(hz, bps / bytes, (service + extra) * cpi, 0, 1, out);
}

int dev_share_fit(double hz, double cycles, double bps, double bytes, DevShare *out) {
    double prepare;
    double service;
    if (!(bps > 0) || !(bytes > 0)) {
        return -1;
    }
    prepare = bytes / bps;
    service = cycles / hz;
    return fill_share(hz, bps / bytes, cycles, prepare, prepare + 1e-15 >= service, out);
}

int dev_share_dma(double hz, double cycles, double bps, double block, DevShare *out) {
    if (!(bps > 0) || !(block > 0)) {
        return -1;
    }
    return fill_share(hz, bps / block, cycles, 0, 1, out);
}

long long dev_dma_requests(long long nbytes, long long buf_bits) {
    long long bits;
    if (nbytes <= 0 || buf_bits <= 0) {
        return -1;
    }
    bits = nbytes * 8;
    if (bits % buf_bits != 0) {
        return -1;
    }
    return bits / buf_bits;
}

int dev_serial(double start, double data, double parity, double stop, double gap_s, DevFrame *out) {
    if (out == NULL || !(gap_s > 0) || start < 0 || data < 0 || parity < 0 || stop < 0) {
        return -1;
    }
    out->bits = start + data + parity + stop;
    out->per_s = 1.0 / gap_s;
    dev_fmt_num(out->bits_text, sizeof out->bits_text, out->bits);
    dev_fmt_num(out->rate_text, sizeof out->rate_text, out->per_s);
    return 0;
}

int dev_task(double hz, double gap_s, double cpi, double resp, double insns, double launch,
             double count, DevTask *out) {
    double prep;
    if (out == NULL || !(hz > 0) || !(gap_s > 0) || !(cpi > 0) || resp < 0 || !(insns > 0) ||
        launch < 0 || launch > insns || !(count > 0)) {
        return -1;
    }
    prep = gap_s * hz;
    out->total = (prep + resp + launch * cpi) * count;
    out->cpu = (resp + insns * cpi) * count;
    dev_fmt_num(out->total_text, sizeof out->total_text, out->total);
    dev_fmt_num(out->cpu_text, sizeof out->cpu_text, out->cpu);
    return 0;
}
