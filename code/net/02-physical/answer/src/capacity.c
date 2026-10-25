#include "capacity.h"

#include "unit.h"

#include <limits.h>
#include <string.h>

bool symbol_from_states(long long states, Symbol *out) {
    int bits;

    if (out == NULL || states < 2) {
        return false;
    }
    bits = floor_log2_u((unsigned long long)states);
    /* 1<<31 起符号左移是未定义行为，考题的状态数也远小于 2^30 */
    if (bits < 1 || bits > 30 || (1LL << bits) != states) {
        return false;
    }
    out->kind = SYM_STATES;
    out->states = states;
    out->bits = bits;
    out->phases = 0;
    out->amps = 0;
    return true;
}

bool symbol_from_phase(int phases, int amps, Symbol *out) {
    long long states;

    if (out == NULL || phases < 1 || amps < 1) {
        return false;
    }
    if (!mul_ll(phases, amps, &states)) {
        return false;
    }
    if (!symbol_from_states(states, out)) {
        return false;
    }
    out->kind = SYM_PHASE;
    out->phases = phases;
    out->amps = amps;
    return true;
}

bool convert_rate(int from_bits, long long rate, const Symbol *sym, ConvertOut *out) {
    if (out == NULL || sym == NULL || rate <= 0 || sym->bits < 1) {
        return false;
    }
    out->symbol = *sym;
    out->from_bits = from_bits;
    if (from_bits) {
        if (rate % sym->bits != 0) {
            return false;
        }
        out->bps = rate;
        out->baud = rate / sym->bits;
        return true;
    }
    if (!mul_ll(rate, sym->bits, &out->bps)) {
        return false;
    }
    out->baud = rate;
    return true;
}

bool convert_code(int from_bits, long long rate, int manchester, CodeOut *out) {
    int per_bit = manchester ? 2 : 1;

    if (out == NULL || rate <= 0) {
        return false;
    }
    out->from_bits = from_bits;
    out->manchester = manchester ? 1 : 0;
    if (from_bits) {
        if (!mul_ll(rate, per_bit, &out->baud)) {
            return false;
        }
        out->bps = rate;
        return true;
    }
    if (rate % per_bit != 0) {
        return false;
    }
    out->baud = rate;
    out->bps = rate / per_bit;
    return true;
}

bool nyquist_compute(long long hz, const Symbol *sym, NyquistOut *out) {
    long long baud;

    if (out == NULL || sym == NULL || hz <= 0 || hz > LLONG_MAX / 2) {
        return false;
    }
    baud = hz * 2;
    if (!mul_ll(baud, sym->bits, &out->max_bps)) {
        return false;
    }
    out->hz = hz;
    out->max_baud = baud;
    out->symbol = *sym;
    return true;
}

bool nyquist_solve(long long hz, long long bps, NyquistOut *out) {
    long long baud;
    long long bits;
    long long states;

    if (out == NULL || hz <= 0 || hz > LLONG_MAX / 2 || bps <= 0) {
        return false;
    }
    baud = hz * 2;
    if (bps % baud != 0) {
        return false;
    }
    bits = bps / baud;
    if (bits < 1 || bits > 30) {
        return false;
    }
    states = 1LL << bits;
    if (!symbol_from_states(states, &out->symbol)) {
        return false;
    }
    out->hz = hz;
    out->max_baud = baud;
    out->max_bps = bps;
    return true;
}

static int within_3_percent(unsigned long long bigger, unsigned long long smaller) {
    if (smaller == 0 || bigger < smaller) {
        return 0;
    }
    if (bigger > ULLONG_MAX / 100 || smaller > ULLONG_MAX / 103) {
        return 0;
    }
    /* 较大 / 较小 ≤ 103/100。30 dB 时 1024/1001 约 1.023，卡在这道门里 */
    return bigger * 100 <= smaller * 103;
}

static int power_of_10(int exp, unsigned long long *out) {
    static const unsigned long long table[] = {
        1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL,
    };

    if (exp < 0 || exp > 6) {
        return 0;
    }
    *out = table[exp];
    return 1;
}

static int classify_snr(unsigned long long snr, ShannonOut *out) {
    unsigned long long one;
    unsigned long long lo;
    unsigned long long hi;
    int floor_bits;

    if (snr < 1 || snr > 1000000ULL) {
        return 0;
    }
    one = snr + 1;
    floor_bits = floor_log2_u(one);
    if (floor_bits < 0 || floor_bits > 20) {
        return 0;
    }
    lo = 1ULL << floor_bits;
    out->snr = snr;
    out->one_plus = one;
    out->floor_bits = floor_bits;
    if (one == lo) {
        out->kind = SHANNON_EXACT;
        out->log_bits = floor_bits;
        out->ceil_bits = floor_bits;
        return 1;
    }
    hi = lo << 1;
    out->ceil_bits = floor_bits + 1;
    if (within_3_percent(hi, one)) {
        out->kind = SHANNON_APPROX;
        out->log_bits = floor_bits + 1;
        return 1;
    }
    if (within_3_percent(one, lo)) {
        out->kind = SHANNON_APPROX;
        out->log_bits = floor_bits;
        return 1;
    }
    out->kind = SHANNON_RANGE;
    out->log_bits = -1;
    return 1;
}

bool shannon_prepare(int use_db, int db, unsigned long long ratio, ShannonOut *out) {
    unsigned long long snr;

    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->log_bits = -1;
    if (use_db) {
        if (db < 0 || db > 60 || db % 10 != 0 || !power_of_10(db / 10, &snr)) {
            return false;
        }
        out->use_db = 1;
        out->db = db;
    } else if (ratio < 1 || ratio > 1000000ULL) {
        return false;
    } else {
        snr = ratio;
        out->db = -1;
    }
    return classify_snr(snr, out);
}

static int apply_percent(long long bps, int percent, long long *out) {
    if (percent < 1 || percent > 100 || bps < 0) {
        return 0;
    }
    if (bps > LLONG_MAX / percent) {
        return 0;
    }
    if ((bps * percent) % 100 != 0) {
        return 0;
    }
    *out = bps * percent / 100;
    return 1;
}

bool shannon_compute(long long hz, int use_db, int db, unsigned long long ratio,
                     int show_actual, int percent, ShannonOut *out) {
    long long theory;

    if (out == NULL || hz <= 0) {
        return false;
    }
    if (!shannon_prepare(use_db, db, ratio, out)) {
        return false;
    }
    out->hz = hz;
    out->show_actual = show_actual ? 1 : 0;
    out->percent = show_actual ? percent : 100;
    if (out->kind == SHANNON_RANGE) {
        return !show_actual;
    }
    if (!mul_ll(hz, out->log_bits, &theory)) {
        return false;
    }
    out->theory_bps = theory;
    if (!show_actual) {
        out->actual_bps = theory;
        return true;
    }
    return apply_percent(theory, percent, &out->actual_bps);
}

bool bound_compute(long long hz, const Symbol *sym, int use_db, int db,
                   unsigned long long ratio, BoundOut *out) {
    if (out == NULL || !nyquist_compute(hz, sym, &out->nyquist)) {
        return false;
    }
    if (!shannon_compute(hz, use_db, db, ratio, 0, 100, &out->shannon)) {
        return false;
    }
    if (out->shannon.kind == SHANNON_RANGE) {
        return false;
    }
    if (out->nyquist.max_bps < out->shannon.theory_bps) {
        out->which = 0;
        out->limit_bps = out->nyquist.max_bps;
    } else if (out->nyquist.max_bps > out->shannon.theory_bps) {
        out->which = 1;
        out->limit_bps = out->shannon.theory_bps;
    } else {
        out->which = 2;
        out->limit_bps = out->nyquist.max_bps;
    }
    return true;
}

static unsigned long long isqrt_ceil(unsigned long long x) {
    unsigned long long lo = 0;
    unsigned long long hi = 1ULL << 20;
    unsigned long long mid;

    if (x <= 1) {
        return x == 0 ? 0 : 1;
    }
    while (lo + 1 < hi) {
        mid = lo + (hi - lo) / 2;
        if (mid > ULLONG_MAX / mid || mid * mid > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    if (lo > 0 && lo * lo == x) {
        return lo;
    }
    return lo + 1;
}

bool states_need(int use_db, int db, unsigned long long ratio, NeedOut *out) {
    unsigned long long root;
    long long pow2;
    int bits;

    if (out == NULL || !shannon_prepare(use_db, db, ratio, &out->info)) {
        return false;
    }
    root = isqrt_ceil(out->info.one_plus);
    if (root < 2 || root > 1000000ULL) {
        return false;
    }
    out->min_v = (long long)root;
    out->prev_v = out->min_v - 1;
    out->prev_sq = out->prev_v * out->prev_v;
    out->min_sq = out->min_v * out->min_v;
    pow2 = 2;
    bits = 1;
    while ((unsigned long long)pow2 < root) {
        if (bits >= 30) {
            return false;
        }
        pow2 <<= 1;
        bits++;
    }
    out->min_pow2 = pow2;
    out->pow_bits = bits;
    return true;
}
