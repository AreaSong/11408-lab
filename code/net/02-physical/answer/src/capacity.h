#ifndef CAPACITY_H
#define CAPACITY_H

#include <stdbool.h>

/*
 * 奈奎斯特：无噪声理想低通，最高码元速率 = 2W，数据率 = 2W log2(V)。
 * 题目没写带通时不用 W。香农：有噪声 C = W log2(1+S/N)，注意是 W 不是 2W。
 * 两个上限都给出时取较小值。30 dB 的 log2(1001) 按王道近似成 10。
 */

typedef enum {
    SYM_STATES = 0,
    SYM_PHASE = 1
} SymKind;

typedef struct {
    SymKind kind;
    long long states;
    int bits;
    int phases;
    int amps;
} Symbol;

typedef enum {
    SHANNON_EXACT = 0,
    SHANNON_APPROX = 1,
    SHANNON_RANGE = 2
} ShannonKind;

typedef struct {
    int from_bits;
    long long bps;
    long long baud;
    Symbol symbol;
} ConvertOut;

typedef struct {
    int from_bits;
    int manchester;
    long long bps;
    long long baud;
} CodeOut;

typedef struct {
    long long hz;
    long long max_baud;
    long long max_bps;
    Symbol symbol;
} NyquistOut;

typedef struct {
    int use_db;
    int db;
    unsigned long long snr;
    unsigned long long one_plus;
    ShannonKind kind;
    int log_bits;
    int floor_bits;
    int ceil_bits;
    long long hz;
    long long theory_bps;
    int show_actual;
    int percent;
    long long actual_bps;
} ShannonOut;

typedef struct {
    NyquistOut nyquist;
    ShannonOut shannon;
    long long limit_bps;
    int which;
} BoundOut;

typedef struct {
    ShannonOut info;
    long long min_v;
    long long prev_v;
    long long prev_sq;
    long long min_sq;
    long long min_pow2;
    int pow_bits;
} NeedOut;

bool symbol_from_states(long long states, Symbol *out);
bool symbol_from_phase(int phases, int amps, Symbol *out);

bool convert_rate(int from_bits, long long rate, const Symbol *sym, ConvertOut *out);
bool convert_code(int from_bits, long long rate, int manchester, CodeOut *out);

bool nyquist_compute(long long hz, const Symbol *sym, NyquistOut *out);
bool nyquist_solve(long long hz, long long bps, NyquistOut *out);

bool shannon_prepare(int use_db, int db, unsigned long long ratio, ShannonOut *out);
bool shannon_compute(long long hz, int use_db, int db, unsigned long long ratio,
                     int show_actual, int percent, ShannonOut *out);

bool bound_compute(long long hz, const Symbol *sym, int use_db, int db,
                   unsigned long long ratio, BoundOut *out);
bool states_need(int use_db, int db, unsigned long long ratio, NeedOut *out);

#endif
