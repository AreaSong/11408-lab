#include "crcdiv.h"

#include "bits.h"

#include <stdio.h>
#include <string.h>

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void copy_window(const char *work, int at, int width, char *dst) {
    memcpy(dst, work + at, (size_t)width);
    dst[width] = '\0';
}

static int parse_binary_gen(const char *text, char *gen, int cap, char *err, int err_cap) {
    int n = (int)strlen(text);
    int i;

    if (n < 2 || n >= cap) {
        lab_err(err, err_cap, "除数至少 2 位，最高次数最大 16");
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (text[i] != '0' && text[i] != '1') {
            lab_err(err, err_cap, "除数里只能有 0 和 1");
            return 0;
        }
    }
    if (text[0] != '1' || text[n - 1] != '1') {
        lab_err(err, err_cap, "生成多项式最高次和常数项都必须是 1");
        return 0;
    }
    memcpy(gen, text, (size_t)n + 1);
    return 1;
}

static int term_degree(const char *term, int n, int *deg) {
    int i = 1;
    int v = 0;

    if (n == 1 && term[0] == '1') {
        *deg = 0;
        return 1;
    }
    if (n < 1 || term[0] != 'x') {
        return 0;
    }
    if (n == 1) {
        *deg = 1;
        return 1;
    }
    if (term[1] == '^') {
        i = 2;
    }
    if (i >= n) {
        return 0;
    }
    for (; i < n; i++) {
        if (term[i] < '0' || term[i] > '9') {
            return 0;
        }
        v = v * 10 + (term[i] - '0');
        if (v > 16) {
            return 0;
        }
    }
    if (v < 1) {
        return 0;
    }
    *deg = v;
    return 1;
}

static int add_term(const char *poly, int from, int to, char *seen, char *err, int err_cap) {
    int deg = 0;

    if (to <= from || !term_degree(poly + from, to - from, &deg)) {
        lab_err(err, err_cap, "生成多项式写成 10011，或 x^4+x+1");
        return 0;
    }
    if (seen[deg]) {
        lab_err(err, err_cap, "同一项写了两次");
        return 0;
    }
    seen[deg] = 1;
    return 1;
}

static int parse_poly_gen(const char *text, char *gen, int cap, char *err, int err_cap) {
    char compact[128];
    char seen[17];
    int n = 0;
    int i;
    int from = 0;
    int deg;
    int max_deg = -1;

    memset(seen, 0, sizeof seen);
    for (i = 0; text[i] != '\0'; i++) {
        if (is_space(text[i])) {
            continue;
        }
        if (n + 1 >= (int)sizeof compact) {
            lab_err(err, err_cap, "生成多项式太长");
            return 0;
        }
        compact[n++] = text[i];
    }
    compact[n] = '\0';
    if (n == 0) {
        lab_err(err, err_cap, "缺少生成多项式");
        return 0;
    }
    for (i = 0; i <= n; i++) {
        if (i < n && compact[i] != '+') {
            continue;
        }
        if (!add_term(compact, from, i, seen, err, err_cap)) {
            return 0;
        }
        from = i + 1;
    }
    for (deg = 16; deg >= 0; deg--) {
        if (seen[deg]) {
            max_deg = deg;
            break;
        }
    }
    if (max_deg < 1 || !seen[0] || max_deg + 1 >= cap) {
        lab_err(err, err_cap, "生成多项式最高次和常数项都必须是 1");
        return 0;
    }
    for (deg = max_deg; deg >= 0; deg--) {
        gen[max_deg - deg] = seen[deg] ? '1' : '0';
    }
    gen[max_deg + 1] = '\0';
    return 1;
}

static int compact_gen(const char *text, char *dst, int cap, char *err, int err_cap) {
    int n = 0;

    for (; *text != '\0'; text++) {
        char c = *text == 'X' ? 'x' : *text;
        if (is_space(c)) {
            continue;
        }
        if (n + 1 >= cap) {
            lab_err(err, err_cap, "生成多项式太长");
            return 0;
        }
        dst[n++] = c;
    }
    dst[n] = '\0';
    if (n == 0) {
        lab_err(err, err_cap, "缺少生成多项式");
        return 0;
    }
    return 1;
}

int crc_parse_gen(const char *text, char *gen, int cap, char *err, int err_cap) {
    char compact[128];
    int i;
    int binary = 1;

    if (!compact_gen(text, compact, (int)sizeof compact, err, err_cap)) {
        return 0;
    }
    for (i = 0; compact[i] != '\0'; i++) {
        if (compact[i] == 'x' || compact[i] == '+' || compact[i] == '^') {
            binary = 0;
        }
    }
    if (binary) {
        return parse_binary_gen(compact, gen, cap, err, err_cap);
    }
    return parse_poly_gen(compact, gen, cap, err, err_cap);
}

static void format_poly(const char *gen, char *out, int cap) {
    int n = (int)strlen(gen);
    int used = 0;
    int i;
    int first = 1;

    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int deg = n - 1 - i;
        int w;

        if (gen[i] != '1') {
            continue;
        }
        if (deg == 0) {
            w = snprintf(out + used, (size_t)(cap - used), "%s1", first ? "" : " + ");
        } else if (deg == 1) {
            w = snprintf(out + used, (size_t)(cap - used), "%sx", first ? "" : " + ");
        } else {
            w = snprintf(out + used, (size_t)(cap - used), "%sx^%d", first ? "" : " + ", deg);
        }
        if (w < 0 || used + w >= cap) {
            return;
        }
        used += w;
        first = 0;
    }
}

static int divide_steps(char *work, int width, int rounds, const char *gen, CrcWork *out) {
    int i;

    if (rounds > CRC_STEP_CAP) {
        return 0;
    }
    for (i = 0; i < rounds; i++) {
        CrcStep *step = &out->steps[i];

        copy_window(work, i, width, step->window);
        step->quotient = work[i] == '1';
        out->quot[i] = step->quotient ? '1' : '0';
        if (step->quotient) {
            int j;
            for (j = 0; j < width; j++) {
                work[i + j] = (work[i + j] == gen[j]) ? '0' : '1';
            }
        }
        copy_window(work, i, width, step->after);
    }
    out->quot[rounds] = '\0';
    out->nsteps = rounds;
    return 1;
}

static int load_divide(const char *bits, int n, const char *gen, char *data, char *gen_out,
                        int *width, char *err, int err_cap) {
    int w = (int)strlen(gen);

    /* 先拷到本地。调用方可能把 bits 指到即将被清零的 CrcWork 里面。 */
    if (n < 1 || n >= CRC_BIT_CAP || w < 2 || w >= CRC_GEN_CAP) {
        lab_err(err, err_cap, "数据和生成多项式都至少要有有效位");
        return 0;
    }
    memcpy(data, bits, (size_t)n);
    data[n] = '\0';
    memcpy(gen_out, gen, (size_t)w + 1);
    *width = w;
    return 1;
}

int crc_divide(const char *bits, int n, const char *gen, int append_zeros,
               CrcWork *out, char *err, int err_cap) {
    int width = 0;
    int r;
    int rounds;
    char data[CRC_BIT_CAP];
    char gen_local[CRC_GEN_CAP];
    char work[CRC_BIT_CAP];

    if (!load_divide(bits, n, gen, data, gen_local, &width, err, err_cap)) {
        return 0;
    }
    r = width - 1;
    if (n + r >= CRC_BIT_CAP) {
        lab_err(err, err_cap, "比特串太长");
        return 0;
    }
    memset(out, 0, sizeof *out);
    memcpy(out->gen, gen_local, (size_t)width + 1);
    format_poly(gen_local, out->poly, (int)sizeof out->poly);
    out->gen_len = width;
    out->r = r;
    out->append_zeros = append_zeros;
    memcpy(out->data, data, (size_t)n + 1);
    out->data_len = n;
    if (!append_zeros && n <= r) {
        lab_err(err, err_cap, "接收串要比余数位数 r 更长");
        return 0;
    }
    memcpy(work, data, (size_t)n);
    rounds = append_zeros ? n : n - r;
    if (append_zeros) {
        memset(work + n, '0', (size_t)r);
    }
    work[n + (append_zeros ? r : 0)] = '\0';
    memcpy(out->dividend, work, (size_t)strlen(work) + 1);
    if (!divide_steps(work, width, rounds, gen_local, out)) {
        lab_err(err, err_cap, "除法步数太多");
        return 0;
    }
    memcpy(out->remainder, work + (append_zeros ? n : n - r), (size_t)r);
    out->remainder[r] = '\0';
    out->clean = strspn(out->remainder, "0") == (size_t)r;
    if (append_zeros) {
        snprintf(out->sent, sizeof out->sent, "%s%s", out->data, out->remainder);
    }
    return 1;
}
