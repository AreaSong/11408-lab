#include "run_dist.h"

#include "dist.h"
#include "line.h"
#include "qty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Dist dist;
static int ready;

static Dist *state(void) {
    if (!ready) {
        memset(&dist, 0, sizeof dist);
        ready = 1;
    }
    return &dist;
}

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static int read_double(const char *s, double *out) {
    char *end = NULL;
    double v = strtod(s, &end);

    if (end == s || *end != '\0' || !(v >= 0)) {
        return -1;
    }
    *out = v;
    return 0;
}

static int take_qty(char **p, int is_bits, int allow_zero, long long *out) {
    char num[32];
    char unit[32];
    double v = 0;
    long long dummy = 0;
    int rc = take_need(p, num, (int)sizeof num, "缺少数值");

    if (rc != 0) {
        return rc;
    }
    rc = take_need(p, unit, (int)sizeof unit, "缺少单位");
    if (rc != 0) {
        return rc;
    }
    if (read_double(num, &v) != 0) {
        return reject("数值读不了");
    }
    if (allow_zero && v == 0) {
        int known = is_bits ? parse_bits(1, unit, &dummy) : parse_bps(1, unit, &dummy);
        if (!known) {
            return reject("单位无法识别");
        }
        *out = 0;
        return 0;
    }
    if (is_bits) {
        if (!parse_bits(v, unit, out) || *out <= 0) {
            return reject("文件大小的单位无法识别");
        }
        return 0;
    }
    if (!parse_bps(v, unit, out) || *out <= 0) {
        return reject("速率的单位无法识别");
    }
    return 0;
}

static int take_people(char **p, int *out) {
    char tok[32];
    int rc = take_need(p, tok, (int)sizeof tok, "缺少人数");

    if (rc != 0) {
        return rc;
    }
    if (parse_u(tok, out, 1000000) != 0 || *out < 1) {
        return reject("人数要是 1 到 1000000 的整数");
    }
    return 0;
}

static const char *cs_why(int mask) {
    if (mask == 3) {
        return "两项一样大。";
    }
    if (mask & 1) {
        return "瓶颈是服务器上传。";
    }
    return "瓶颈是最慢对等方的下载。";
}

static const char *p2p_why(int mask) {
    if (mask == 1) {
        return "瓶颈是服务器把整份文件送进网络的这一次。";
    }
    if (mask == 2) {
        return "瓶颈是最慢对等方的下载。";
    }
    if (mask == 4) {
        return "瓶颈是系统的总上传。";
    }
    return "有多项同时达到这个时间。";
}

static void print_cs(FILE *fp, const Dist *d, const DistOut *o) {
    char send[80];
    char down[80];
    char all[80];

    ratio_fmt(send, sizeof send, o->nf_us);
    ratio_fmt(down, sizeof down, o->f_dmin);
    ratio_fmt(all, sizeof all, o->cs);
    fprintf(fp, "C/S：NF/us = %d × %lld bit / %lld bit/s = %s。\n", d->n, d->file_bits, d->server_bps, send);
    fprintf(fp, "F/dmin = %lld bit / %lld bit/s = %s。\n", d->file_bits, d->dmin, down);
    fprintf(fp, "Dcs = max(NF/us, F/dmin) = %s。%s\n", all, cs_why(o->cs_mask));
}

static void print_p2p(FILE *fp, const DistOut *o) {
    char seed[80];
    char down[80];
    char total[80];
    char all[80];

    ratio_fmt(seed, sizeof seed, o->f_us);
    ratio_fmt(down, sizeof down, o->f_dmin);
    ratio_fmt(total, sizeof total, o->nf_total);
    ratio_fmt(all, sizeof all, o->p2p);
    fprintf(fp, "P2P：服务器至少把每个比特送出一次，F/us = %s。\n", seed);
    fprintf(fp, "F/dmin = %s。\n", down);
    fprintf(fp, "总上传 us+Σui = %lld bit/s。NF/(us+Σui) = %s。\n", o->total_up, total);
    fprintf(fp, "Dp2p = max(F/us, F/dmin, NF/(us+Σui)) = %s。%s\n", all, p2p_why(o->p2p_mask));
}

static void print_result(FILE *fp, const DistOut *o) {
    char cs[80];
    char p2p[80];

    ratio_fmt(cs, sizeof cs, o->cs);
    ratio_fmt(p2p, sizeof p2p, o->p2p);
    fprintf(fp, "结果：C/S %s，P2P %s\n", cs, p2p);
}

static int cmd_go(void) {
    DistOut out;
    const char *err = dist_eval(state(), &out);
    char file[96];
    char server[96];
    char down[96];
    char up[96];
    const Dist *d = state();

    if (err != NULL) {
        return reject(err);
    }
    fmt_span(file, sizeof file, d->file_bits);
    fmt_bps(server, sizeof server, d->server_bps);
    fmt_bps(down, sizeof down, d->dmin);
    fmt_bps(up, sizeof up, d->sum_up);
    output_begin();
    qty_print_scale_note(stdout);
    fprintf(stdout, "文件 F = %s。\n", file);
    fprintf(stdout, "服务器上传 us = %s。\n", server);
    fprintf(stdout, "对等方 N = %d。最慢下载 dmin = %s。上传之和 Σui = %s。\n", d->n, down, up);
    if (!d->uniform) {
        fputs("对等方速率不完全相同：dmin 取最慢的下载，Σui 把上传加起来。\n", stdout);
    }
    print_cs(stdout, d, &out);
    print_p2p(stdout, &out);
    if (d->sum_up == 0) {
        fputs("对等方都不上传，us+Σui 等于 us，P2P 的第三项和 C/S 的 NF/us 相同。\n", stdout);
    }
    print_result(stdout, &out);
    return 0;
}

static void print_limit(const Dist *d) {
    Ratio lim;
    char buf[80];

    if (d->one_up <= 0) {
        fputs("这些对等方的上传是 0。P2P 的总上传只剩 us，NF/(us+Σui) 和 C/S 的 NF/us 是同一项。\n", stdout);
        return;
    }
    if (ratio_make(d->file_bits, d->one_up, &lim) != 0) {
        return;
    }
    ratio_fmt(buf, sizeof buf, lim);
    fprintf(stdout, "每个对等方上传都是 u 时，NF/(us+Nu) 随 N 增大趋近 F/u = %s。\n", buf);
    fputs("C/S 的 NF/us 随 N 线性变大，没有这个上界。\n", stdout);
}

static int read_ns(char **p, int *ns, int *count) {
    *count = 0;
    while (*count < 6) {
        char tok[32];
        int rc = take_token(p, tok, (int)sizeof tok);

        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            return reject("这一项太长");
        }
        if (parse_u(tok, &ns[*count], 1000000) != 0 || ns[*count] < 1) {
            return reject("人数要是 1 到 1000000 的整数");
        }
        (*count)++;
    }
    if (*count == 0) {
        return reject("curve 后面要写人数");
    }
    if (!rest_blank(*p)) {
        return reject("curve 最多 6 个 N");
    }
    return 0;
}

static int print_curve_row(int n, char *cs, int cscap, char *p2p, int p2pcap) {
    Dist scaled;
    DistOut out;
    const char *err = dist_scale(state(), n, &scaled);

    if (err == NULL) {
        scaled.file_bits = state()->file_bits;
        scaled.server_bps = state()->server_bps;
        err = dist_eval(&scaled, &out);
    }
    if (err != NULL) {
        return reject(err);
    }
    ratio_fmt(cs, (size_t)cscap, out.cs);
    ratio_fmt(p2p, (size_t)p2pcap, out.p2p);
    fprintf(stdout, "N = %d：C/S %s，P2P %s\n", n, cs, p2p);
    return 0;
}

static int cmd_curve(char **p) {
    int ns[6];
    int count = 0;
    int i;
    char file[96];
    char server[96];
    char down[96];
    char up[96];
    char cs[80];
    char p2p[80];
    const Dist *d = state();

    if (read_ns(p, ns, &count) != 0) {
        return -1;
    }
    if (d->file_bits <= 0 || d->server_bps <= 0 || d->n < 1) {
        return reject("要先写 file、server 和 peer");
    }
    if (!d->uniform) {
        return reject("curve 要求每个对等方的上下载速率相同");
    }
    fmt_span(file, sizeof file, d->file_bits);
    fmt_bps(server, sizeof server, d->server_bps);
    fmt_bps(down, sizeof down, d->one_down);
    fmt_bps(up, sizeof up, d->one_up);
    output_begin();
    qty_print_scale_note(stdout);
    fprintf(stdout, "文件 F = %s。\n", file);
    fprintf(stdout, "服务器上传 us = %s。\n", server);
    fprintf(stdout, "每个对等方下载 %s，上传 %s。下表把这一套速率复制 N 份。\n", down, up);
    fputs("Dcs = max(NF/us, F/dmin)，Dp2p = max(F/us, F/dmin, NF/(us+Σui))。\n", stdout);
    for (i = 0; i < count; i++) {
        if (print_curve_row(ns[i], cs, (int)sizeof cs, p2p, (int)sizeof p2p) != 0) {
            return -1;
        }
    }
    print_limit(d);
    fprintf(stdout, "结果：N = %d 时 C/S %s，P2P %s\n", ns[count - 1], cs, p2p);
    return 0;
}

static int cmd_peer(char **p) {
    long long down = 0;
    long long up = 0;
    const char *err;

    if (take_qty(p, 0, 0, &down) != 0 || take_qty(p, 0, 1, &up) != 0) {
        return -1;
    }
    err = dist_add(state(), down, up);
    if (err != NULL) {
        return reject(err);
    }
    return need_end(*p);
}

static int cmd_count(char **p) {
    int n = 0;
    const char *err;

    if (take_people(p, &n) != 0) {
        return -1;
    }
    err = dist_count(state(), n);
    if (err != NULL) {
        return reject(err);
    }
    return need_end(*p);
}

static int cmd_file(char **p, int is_bits) {
    long long v = 0;

    if (take_qty(p, is_bits, 0, &v) != 0) {
        return -1;
    }
    if (is_bits) {
        state()->file_bits = v;
    } else {
        state()->server_bps = v;
    }
    return need_end(*p);
}

int run_dist(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_need(&p, cmd, (int)sizeof cmd, "缺少命令");

    if (rc != 0) {
        return rc;
    }
    if (strcmp(cmd, "reset") == 0) {
        memset(&dist, 0, sizeof dist);
        ready = 1;
        return need_end(p);
    }
    if (strcmp(cmd, "file") == 0) {
        return cmd_file(&p, 1);
    }
    if (strcmp(cmd, "server") == 0) {
        return cmd_file(&p, 0);
    }
    if (strcmp(cmd, "peer") == 0) {
        return cmd_peer(&p);
    }
    if (strcmp(cmd, "count") == 0) {
        return cmd_count(&p);
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(p) != 0) {
            return -1;
        }
        return cmd_go();
    }
    if (strcmp(cmd, "curve") == 0) {
        return cmd_curve(&p);
    }
    return reject("命令用 file、server、peer、count、go、curve 或 reset");
}
