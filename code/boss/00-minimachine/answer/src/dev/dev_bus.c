#include "dev/dev_bus.h"

#include "dev/dev_fmt.h"

static int positive(double x) { return x > 0; }

int dev_bw(double hz, double clocks, double beats, double bits, double duplex, double channels,
           DevBw *out) {
    if (out == NULL || !positive(hz) || !positive(clocks) || !positive(beats) || !positive(bits) ||
        !positive(duplex) || !positive(channels)) {
        return -1;
    }
    out->bps = hz * (beats / clocks) * (bits / 8.0) * duplex * channels;
    dev_fmt_rate(out->brief, sizeof out->brief, out->bps);
    return 0;
}

int dev_txn(int each, double hz, double times, double addr, double wait, double data, double prep,
            double bits, double payload, DevTxn *out) {
    double reps;
    double data_bytes;
    if (out == NULL || !positive(hz) || !positive(bits) || !positive(payload) || !positive(data) ||
        addr < 0 || wait < 0 || prep < 0 || (each && !positive(times))) {
        return -1;
    }
    reps = each ? times : 1.0;
    out->seconds = reps * ((addr + wait + data) / hz + prep);
    if (!(out->seconds > 0)) {
        return -1;
    }
    data_bytes = each ? payload / reps : payload;
    out->avg_bps = payload / out->seconds;
    out->peak_bps = data_bytes / data * hz;
    dev_fmt_time(out->time_brief, sizeof out->time_brief, out->seconds);
    dev_fmt_rate(out->avg_brief, sizeof out->avg_brief, out->avg_bps);
    dev_fmt_rate(out->peak_brief, sizeof out->peak_brief, out->peak_bps);
    return 0;
}
