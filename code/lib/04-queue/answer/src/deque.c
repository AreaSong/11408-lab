#include "deque.h"

#include <string.h>

/* 下标留在中间，左端入队向左长，右端入队向右长。n 不超过 12，两端都够。 */
#define SPAN 40
#define MID 16

typedef struct {
    int buf[SPAN];
    int lo;
    int hi;
    int next;
} Dq;

typedef struct {
    char kind;
    int value;
} Step;

static void dq_init(Dq *d) {
    d->lo = MID;
    d->hi = MID;
    d->next = 1;
}

static int dq_len(const Dq *d) {
    return d->hi - d->lo;
}

static void dq_show(const Dq *d, FILE *out) {
    fputc('[', out);
    for (int i = d->lo; i < d->hi; i++) {
        if (i > d->lo) {
            fputc(' ', out);
        }
        fprintf(out, "%d", d->buf[i]);
    }
    fputc(']', out);
}

static int is_perm(int n, const int *seq) {
    char seen[DEQUE_BATCH_MAX + 1];
    if (seq == NULL || n < 1 || n > DEQUE_BATCH_MAX) {
        return 0;
    }
    memset(seen, 0, (size_t)n + 1);
    for (int i = 0; i < n; i++) {
        if (seq[i] < 1 || seq[i] > n || seen[seq[i]]) {
            return 0;
        }
        seen[seq[i]] = 1;
    }
    return 1;
}

static void show_line(FILE *out, const char *verb, int e, const Dq *d) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "%s e=%d q=", verb, e);
    dq_show(d, out);
    fputc('\n', out);
}

static void show_stuck(FILE *out, const Dq *d) {
    if (out == NULL) {
        return;
    }
    if (dq_len(d) == 0) {
        fprintf(out, "stuck empty\nlegal=0\n");
        return;
    }
    fprintf(out, "stuck front=%d rear=%d q=", d->buf[d->lo], d->buf[d->hi - 1]);
    dq_show(d, out);
    fprintf(out, "\nlegal=0\n");
}

static int at_end(const Dq *d, int want) {
    if (dq_len(d) == 0) {
        return 0;
    }
    return d->buf[d->lo] == want || d->buf[d->hi - 1] == want;
}

static void take_end(Dq *d, int want, FILE *out) {
    if (dq_len(d) > 0 && d->buf[d->lo] == want) {
        int value = d->buf[d->lo++];
        show_line(out, "deq front", value, d);
        return;
    }
    int value = d->buf[--d->hi];
    show_line(out, "deq rear", value, d);
}

bool deque_in_check(int n, const int *seq, FILE *out) {
    Dq d;
    if (n < 1 || n > DEQUE_CHECK_MAX || !is_perm(n, seq)) {
        return false;
    }
    dq_init(&d);
    for (int i = 0; i < n; i++) {
        int want = seq[i];
        int value;
        if (out != NULL) {
            fprintf(out, "want=%d\n", want);
        }
        while (!at_end(&d, want)) {
            if (d.next > n) {
                show_stuck(out, &d);
                return false;
            }
            value = d.next++;
            d.buf[d.hi++] = value;
            show_line(out, "enq right", value, &d);
        }
        take_end(&d, want, out);
    }
    if (out != NULL) {
        fprintf(out, "legal=1\n");
    }
    return true;
}

static void replay(const Step *steps, int nsteps, FILE *out) {
    Dq d;
    dq_init(&d);
    for (int i = 0; i < nsteps; i++) {
        const char *verb = "deq front";
        if (steps[i].kind == 'R') {
            d.buf[d.hi++] = steps[i].value;
            verb = "enq right";
        } else if (steps[i].kind == 'L') {
            d.buf[--d.lo] = steps[i].value;
            verb = "enq left";
        } else {
            d.lo++;
        }
        show_line(out, verb, steps[i].value, &d);
    }
}

static bool out_try(Dq d, const int *seq, int n, int got, Step *steps, int ns, Step *win, int *wn) {
    if (got == n) {
        memcpy(win, steps, (size_t)ns * sizeof(Step));
        *wn = ns;
        return true;
    }
    if (dq_len(&d) > 0 && d.buf[d.lo] == seq[got]) {
        Dq nxt = d;
        steps[ns].kind = 'F';
        steps[ns].value = nxt.buf[nxt.lo];
        nxt.lo++;
        if (out_try(nxt, seq, n, got + 1, steps, ns + 1, win, wn)) {
            return true;
        }
    }
    if (d.next > n) {
        return false;
    }
    steps[ns].value = d.next;
    steps[ns].kind = 'R';
    {
        Dq right = d;
        right.buf[right.hi++] = d.next;
        right.next = d.next + 1;
        if (out_try(right, seq, n, got, steps, ns + 1, win, wn)) {
            return true;
        }
    }
    steps[ns].kind = 'L';
    {
        Dq left = d;
        left.buf[--left.lo] = d.next;
        left.next = d.next + 1;
        return out_try(left, seq, n, got, steps, ns + 1, win, wn);
    }
}

bool deque_out_check(int n, const int *seq, FILE *out) {
    Dq d;
    Step steps[DEQUE_CHECK_MAX * 2];
    Step win[DEQUE_CHECK_MAX * 2];
    int wn = 0;
    if (n < 1 || n > DEQUE_CHECK_MAX || !is_perm(n, seq)) {
        return false;
    }
    dq_init(&d);
    if (!out_try(d, seq, n, 0, steps, 0, win, &wn)) {
        if (out != NULL) {
            fprintf(out, "legal=0\n");
        }
        return false;
    }
    if (out != NULL) {
        replay(win, wn, out);
        fprintf(out, "legal=1\n");
    }
    return true;
}

static bool batch_try(Dq d, const int *seq, int n, Step *steps, int ns, Step *win, int *wn) {
    if (d.next > n) {
        if (dq_len(&d) != n) {
            return false;
        }
        for (int i = 0; i < n; i++) {
            if (d.buf[d.lo + i] != seq[i]) {
                return false;
            }
        }
        memcpy(win, steps, (size_t)ns * sizeof(Step));
        *wn = ns;
        return true;
    }
    steps[ns].value = d.next;
    steps[ns].kind = 'R';
    {
        Dq right = d;
        right.buf[right.hi++] = d.next;
        right.next = d.next + 1;
        if (batch_try(right, seq, n, steps, ns + 1, win, wn)) {
            return true;
        }
    }
    steps[ns].kind = 'L';
    {
        Dq left = d;
        left.buf[--left.lo] = d.next;
        left.next = d.next + 1;
        return batch_try(left, seq, n, steps, ns + 1, win, wn);
    }
}

static void print_out_line(FILE *out, int n, const int *seq) {
    fprintf(out, "out=");
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fprintf(out, "%d", seq[i]);
    }
    fputc('\n', out);
}

bool deque_batch_check(int n, const int *seq, FILE *out) {
    Dq d;
    Step steps[DEQUE_BATCH_MAX];
    Step win[DEQUE_BATCH_MAX];
    int wn = 0;
    if (n < 1 || n > DEQUE_BATCH_MAX || !is_perm(n, seq)) {
        return false;
    }
    dq_init(&d);
    if (!batch_try(d, seq, n, steps, 0, win, &wn)) {
        if (out != NULL) {
            fprintf(out, "legal=0\n");
        }
        return false;
    }
    if (out != NULL) {
        replay(win, wn, out);
        print_out_line(out, n, seq);
        fprintf(out, "legal=1\n");
    }
    return true;
}

static void fill_perms(int n, int *used, int *cur, int len, int seqs[][DEQUE_ENUM_MAX], int *count) {
    if (len == n) {
        memcpy(seqs[*count], cur, (size_t)n * sizeof(int));
        (*count)++;
        return;
    }
    for (int v = 1; v <= n; v++) {
        if (used[v]) {
            continue;
        }
        used[v] = 1;
        cur[len] = v;
        fill_perms(n, used, cur, len + 1, seqs, count);
        used[v] = 0;
    }
}

static void print_bucket(FILE *out, const char *title, int n, int seqs[][DEQUE_ENUM_MAX],
                         const char *kinds, int count, char want) {
    fprintf(out, "%s\n", title);
    for (int i = 0; i < count; i++) {
        if (kinds[i] != want) {
            continue;
        }
        for (int j = 0; j < n; j++) {
            if (j > 0) {
                fputc(' ', out);
            }
            fprintf(out, "%d", seqs[i][j]);
        }
        fputc('\n', out);
    }
}

static char kind_of(int n, const int *seq) {
    int in_ok = deque_in_check(n, seq, NULL);
    int out_ok = deque_out_check(n, seq, NULL);
    if (in_ok && out_ok) {
        return 'B';
    }
    if (in_ok) {
        return 'I';
    }
    if (out_ok) {
        return 'O';
    }
    return 'N';
}

int deque_enum(int n, FILE *out) {
    int seqs[120][DEQUE_ENUM_MAX];
    char kinds[120];
    int used[DEQUE_ENUM_MAX + 1];
    int cur[DEQUE_ENUM_MAX];
    int count = 0;
    int both = 0;
    int only_in = 0;
    int only_out = 0;
    int neither = 0;
    int batch = 0;
    if (out == NULL || n < 1 || n > DEQUE_ENUM_MAX) {
        return -1;
    }
    memset(used, 0, sizeof used);
    fill_perms(n, used, cur, 0, seqs, &count);
    for (int i = 0; i < count; i++) {
        kinds[i] = kind_of(n, seqs[i]);
        if (kinds[i] == 'B') {
            both++;
        } else if (kinds[i] == 'I') {
            only_in++;
        } else if (kinds[i] == 'O') {
            only_out++;
        } else {
            neither++;
        }
        if (deque_batch_check(n, seqs[i], NULL)) {
            batch++;
        }
    }
    fprintf(out, "case enum n=%d\n", n);
    print_bucket(out, "both", n, seqs, kinds, count, 'B');
    print_bucket(out, "only_in", n, seqs, kinds, count, 'I');
    print_bucket(out, "only_out", n, seqs, kinds, count, 'O');
    print_bucket(out, "neither", n, seqs, kinds, count, 'N');
    fprintf(out, "count both=%d only_in=%d only_out=%d neither=%d perm=%d batch=%d\n", both, only_in,
            only_out, neither, count, batch);
    return 0;
}
