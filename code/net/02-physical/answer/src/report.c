#include "report.h"

#include "unit.h"

static void report_symbol(FILE *out, const Symbol *s) {
    if (s->kind == SYM_PHASE) {
        fprintf(out, "相位 = %d，每种相位的振幅 = %d\n", s->phases, s->amps);
        fprintf(out, "码元状态数 V = 相位 × 振幅 = %d × %d = %lld\n",
                s->phases, s->amps, s->states);
    } else {
        fprintf(out, "码元状态数 V = %lld\n", s->states);
    }
    fprintf(out, "每个码元携带 log2(%lld) = %d bit\n", s->states, s->bits);
}

static void report_snr(FILE *out, const ShannonOut *s) {
    if (s->use_db) {
        fprintf(out, "信噪比 = %d dB = 10 × log10(S/N)\n", s->db);
        fprintf(out, "S/N = 10^(%d/10) = %llu\n", s->db, s->snr);
    } else {
        fprintf(out, "信噪比 S/N = %llu（题目给的是比值，不是分贝）\n", s->snr);
    }
    fprintf(out, "1 + S/N = %llu\n", s->one_plus);
}

static void report_approx(FILE *out, const ShannonOut *s, unsigned long long lo,
                          unsigned long long hi) {
    unsigned long long bigger = hi;
    unsigned long long smaller = s->one_plus;

    if (s->log_bits == s->floor_bits) {
        bigger = s->one_plus;
        smaller = lo;
    }
    fprintf(out, "%llu × 100 = %llu，%llu × 103 = %llu，前者不大于后者。\n",
            bigger, bigger * 100ULL, smaller, smaller * 103ULL);
    fprintf(out, "相对差距在 3%% 以内，王道把 log2(%llu) 近似成 %d。\n",
            s->one_plus, s->log_bits);
    if (s->one_plus == 1001ULL) {
        fputs("这就是 30 dB 的常用算法：2^10 = 1024，把 log2(1001) 当成 10。\n", out);
    }
}

static void report_log(FILE *out, const ShannonOut *s) {
    unsigned long long lo = 1ULL << s->floor_bits;

    if (s->kind == SHANNON_EXACT) {
        fprintf(out, "log2(1 + S/N) = log2(%llu) = %d，正好是整数，不用近似。\n",
                s->one_plus, s->log_bits);
        return;
    }
    fprintf(out, "2^%d = %llu，2^%d = %llu\n", s->floor_bits, lo, s->ceil_bits, lo << 1);
    if (s->kind == SHANNON_RANGE) {
        fprintf(out, "%llu 离这两个 2 的幂都超过 3%%，log2 不收成整数。\n", s->one_plus);
        return;
    }
    report_approx(out, s, lo, lo << 1);
}

static void report_range(FILE *out, const ShannonOut *s) {
    long long lo_bps = 0;
    long long hi_bps = 0;
    char lo_buf[128];
    char hi_buf[128];

    if (mul_ll(s->hz, s->floor_bits, &lo_bps) && mul_ll(s->hz, s->ceil_bits, &hi_bps)) {
        fmt_bps(lo_buf, sizeof lo_buf, lo_bps);
        fmt_bps(hi_buf, sizeof hi_buf, hi_bps);
        fprintf(out, "香农极限严格大于 %s，严格小于 %s。\n", lo_buf, hi_buf);
    }
    fputs("结果：不是整数比特，不给单值\n", out);
}

void report_convert(FILE *out, const ConvertOut *r) {
    char bps[128];
    char baud[128];

    fmt_bps(bps, sizeof bps, r->bps);
    fmt_baud(baud, sizeof baud, r->baud);
    fputs("【码元速率与比特率】\n", out);
    unit_print_scale_note(out);
    fputs("比特率 = 波特率 × 每个码元的比特数。波特率就是码元速率。\n", out);
    report_symbol(out, &r->symbol);
    if (r->from_bits) {
        fprintf(out, "数据传输速率 = %s\n", bps);
        fprintf(out, "波特率 = 比特率 / 每码元比特数 = %lld / %d = %s\n",
                r->bps, r->symbol.bits, baud);
    } else {
        fprintf(out, "波特率 = %s\n", baud);
        fprintf(out, "数据传输速率 = 波特率 × 每码元比特数 = %lld × %d = %s\n",
                r->baud, r->symbol.bits, bps);
    }
    fprintf(out, "结果：%s，%s\n", bps, baud);
}

void report_code(FILE *out, const CodeOut *r) {
    char bps[128];
    char baud[128];
    int per = r->manchester ? 2 : 1;

    fmt_bps(bps, sizeof bps, r->bps);
    fmt_baud(baud, sizeof baud, r->baud);
    fputs("【编码的码元速率】\n", out);
    unit_print_scale_note(out);
    if (r->manchester) {
        fputs("曼彻斯特把一个比特分成前后两半，每一半是一个码元，正中间一定跳变。\n", out);
        fputs("码元速率 = 2 × 比特率。同样带宽下，数据率只有不归零制的一半。\n", out);
    } else {
        fputs("NRZ（不归零）一个比特就是一个码元，电平在整个比特里保持不变。\n", out);
        fputs("码元速率 = 比特率。\n", out);
    }
    fprintf(out, "比特率 = %s\n", bps);
    fprintf(out, "每个比特的码元数 = %d\n", per);
    if (r->from_bits) {
        fprintf(out, "码元速率 = %d × %lld bps = %s\n", per, r->bps, baud);
    } else {
        fprintf(out, "码元速率 = %s\n", baud);
        fprintf(out, "比特率 = 码元速率 / %d = %s\n", per, bps);
    }
    fprintf(out, "结果：比特率 %s，码元速率 %s\n", bps, baud);
}

static void report_lowpass(FILE *out) {
    fputs("无噪声、理想低通：最高码元速率 = 2W。理想带通才是 W；题目没写带通，这里用 2W。\n", out);
}

void report_nyquist(FILE *out, const NyquistOut *r) {
    char hz[128];
    char baud[128];
    char bps[128];

    fmt_hz(hz, sizeof hz, r->hz);
    fmt_baud(baud, sizeof baud, r->max_baud);
    fmt_bps(bps, sizeof bps, r->max_bps);
    fputs("【奈奎斯特定理】\n", out);
    unit_print_scale_note(out);
    report_lowpass(out);
    fprintf(out, "带宽 W = %s\n", hz);
    report_symbol(out, &r->symbol);
    fprintf(out, "最高码元速率 = 2 × %lld Hz = %s\n", r->hz, baud);
    fprintf(out, "最大数据传输速率 = %lld Baud × %d bit/码元 = %s\n",
            r->max_baud, r->symbol.bits, bps);
    fprintf(out, "结果：%s\n", bps);
}

void report_solve(FILE *out, const NyquistOut *r) {
    char hz[128];
    char baud[128];
    char bps[128];

    fmt_hz(hz, sizeof hz, r->hz);
    fmt_baud(baud, sizeof baud, r->max_baud);
    fmt_bps(bps, sizeof bps, r->max_bps);
    fputs("【奈奎斯特定理 · 反求状态数】\n", out);
    unit_print_scale_note(out);
    report_lowpass(out);
    fprintf(out, "带宽 W = %s\n", hz);
    fprintf(out, "最大数据传输速率 = %s\n", bps);
    fprintf(out, "最高码元速率 = 2 × %lld Hz = %s\n", r->hz, baud);
    fprintf(out, "每个码元比特数 = %lld / %lld = %d bit\n",
            r->max_bps, r->max_baud, r->symbol.bits);
    fprintf(out, "状态数 V = 2^%d = %lld\n", r->symbol.bits, r->symbol.states);
    if (r->symbol.states >= 4) {
        fprintf(out, "QAM-%lld 用 %lld 种振幅和相位的组合，每个码元 %d bit。\n",
                r->symbol.states, r->symbol.states, r->symbol.bits);
        fprintf(out, "结果：QAM-%lld\n", r->symbol.states);
        return;
    }
    fprintf(out, "结果：状态数 %lld\n", r->symbol.states);
}

void report_shannon(FILE *out, const ShannonOut *s) {
    char hz[128];
    char theory[128];
    char actual[128];
    const char *mark = s->kind == SHANNON_APPROX ? "≈" : "=";
    const char *about = s->kind == SHANNON_APPROX ? "约 " : "";

    fmt_hz(hz, sizeof hz, s->hz);
    fputs("【香农定理】\n", out);
    unit_print_scale_note(out);
    fputs("有噪声信道的极限 C = W × log2(1 + S/N)。这里是 W，不是奈奎斯特的 2W。\n", out);
    fprintf(out, "带宽 W = %s\n", hz);
    report_snr(out, s);
    report_log(out, s);
    if (s->kind == SHANNON_RANGE) {
        report_range(out, s);
        return;
    }
    fmt_bps(theory, sizeof theory, s->theory_bps);
    fprintf(out, "香农极限 %s %lld Hz × %d bit = %s\n", mark, s->hz, s->log_bits, theory);
    if (!s->show_actual) {
        fprintf(out, "结果：%s%s\n", about, theory);
        return;
    }
    fmt_bps(actual, sizeof actual, s->actual_bps);
    fprintf(out, "实际速率 = %d%% × 香农极限 = %s\n", s->percent, actual);
    fputs("题目若还写了「实际约为理论的百分之几」，理论值要再乘这个比例。\n", out);
    fprintf(out, "结果：%s%s\n", about, actual);
}

void report_bound(FILE *out, const BoundOut *b) {
    char hz[128];
    char nb[128];
    char sb[128];
    char lb[128];
    const char *mark = b->shannon.kind == SHANNON_APPROX ? "≈" : "=";
    const char *who = "两者相等";
    int approx_limit = b->shannon.kind == SHANNON_APPROX && b->which != 0;

    if (b->which == 0) {
        who = "奈奎斯特更小";
    } else if (b->which == 1) {
        who = "香农更小";
    }
    fmt_hz(hz, sizeof hz, b->nyquist.hz);
    fmt_bps(nb, sizeof nb, b->nyquist.max_bps);
    fmt_bps(sb, sizeof sb, b->shannon.theory_bps);
    fmt_bps(lb, sizeof lb, b->limit_bps);
    fputs("【两个极限取较小值】\n", out);
    unit_print_scale_note(out);
    fputs("奈奎斯特是无噪声上限，香农是有噪声上限。两个都给出时，不能超过较小的那个。\n", out);
    fprintf(out, "带宽 W = %s\n", hz);
    report_symbol(out, &b->nyquist.symbol);
    fprintf(out, "奈奎斯特极限 = 2 × %lld Hz × %d bit = %s\n",
            b->nyquist.hz, b->nyquist.symbol.bits, nb);
    report_snr(out, &b->shannon);
    report_log(out, &b->shannon);
    fprintf(out, "香农极限 %s %lld Hz × %d bit = %s\n",
            mark, b->shannon.hz, b->shannon.log_bits, sb);
    fprintf(out, "%s。\n", who);
    fprintf(out, "结果：%s%s\n", approx_limit ? "约 " : "", lb);
}

static void report_square(FILE *out, const NeedOut *n) {
    fprintf(out, "%lld^2 = %lld < %llu，%lld 种状态不够\n",
            n->prev_v, n->prev_sq, n->info.one_plus, n->prev_v);
    fprintf(out, "%lld^2 = %lld ≥ %llu，%lld 种状态够\n",
            n->min_v, n->min_sq, n->info.one_plus, n->min_v);
}

static void report_half(FILE *out, const NeedOut *n) {
    int half;
    long long need;

    if (n->info.log_bits < 0 || n->info.log_bits % 2 != 0) {
        return;
    }
    half = n->info.log_bits / 2;
    need = 1LL << half;
    fprintf(out, "2 × log2(V) ≥ %d，log2(V) ≥ %d，V ≥ 2^%d = %lld。\n",
            n->info.log_bits, half, half, need);
    if (need == n->min_v) {
        fputs("和平方比较得到的最少状态数相同。\n", out);
    } else {
        fputs("这是 log2 收成整数之后的算法。精确条件仍是 V^2 ≥ 1+S/N。\n", out);
    }
}

void report_states(FILE *out, const NeedOut *n) {
    fputs("【两个极限 · 最少信号状态数】\n", out);
    unit_print_scale_note(out);
    fputs("无噪声极限 = 2W × log2(V)，有噪声极限 = W × log2(1+S/N)。\n", out);
    fputs("要求无噪声极限不低于有噪声极限时，W 约掉，变成 V^2 ≥ 1+S/N。\n", out);
    report_snr(out, &n->info);
    report_log(out, &n->info);
    report_square(out, n);
    report_half(out, n);
    if (n->min_v == n->min_pow2) {
        fprintf(out, "%lld 本身是 2 的幂。\n", n->min_v);
        fprintf(out, "结果：信号状态数至少 %lld\n", n->min_v);
        return;
    }
    fprintf(out, "凑成 2 的幂至少 %lld = 2^%d。\n", n->min_pow2, n->pow_bits);
    fprintf(out, "结果：整数至少 %lld，2 的幂至少 %lld\n", n->min_v, n->min_pow2);
}
