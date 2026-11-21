#include "kernel/k_vm.h"

#include <stdio.h>
#include <string.h>

int k_ws(const int *pages, int n, int window, int *uniq, int *nu) {
    int start;
    int i;
    int j;
    if (pages == NULL || uniq == NULL || nu == NULL || n < 1 || window < 1) {
        return -1;
    }
    start = n > window ? n - window : 0;
    *nu = 0;
    for (i = start; i < n; i++) {
        int seen = 0;
        for (j = 0; j < *nu; j++) {
            if (uniq[j] == pages[i]) {
                seen = 1;
            }
        }
        if (!seen) {
            uniq[(*nu)++] = pages[i];
        }
    }
    for (i = 1; i < *nu; i++) {
        int value = uniq[i];
        j = i;
        while (j > 0 && uniq[j - 1] > value) {
            uniq[j] = uniq[j - 1];
            j--;
        }
        uniq[j] = value;
    }
    return 0;
}

static int distinct_of(const int *pages, int n) {
    int seen[K_VM_REFS];
    int nd = 0;
    int i;
    int j;
    for (i = 0; i < n; i++) {
        int found = 0;
        for (j = 0; j < nd; j++) {
            if (seen[j] == pages[i]) {
                found = 1;
            }
        }
        if (!found && nd < K_VM_REFS) {
            seen[nd++] = pages[i];
        }
    }
    return nd;
}

static int faults_at(const char *algo, const int *pages, int n, int frames) {
    KAccess refs[K_VM_REFS];
    KReport report;
    int i;
    for (i = 0; i < n; i++) {
        refs[i].page = pages[i];
        refs[i].write = 0;
    }
    if (k_repl_run(algo, frames, NULL, 0, refs, n, &report) != 0) {
        return -1;
    }
    return report.faults;
}

int k_thrash_jump(const char *algo, const int *pages, int n, int *jump) {
    int high;
    int prev = -1;
    int frames;
    if (pages == NULL || jump == NULL || n < 1 || n > K_VM_REFS) {
        return -1;
    }
    high = distinct_of(pages, n);
    if (high > 6) {
        high = 6;
    }
    *jump = 0;
    for (frames = high; frames >= 1; frames--) {
        int faults = faults_at(algo, pages, n, frames);
        if (faults < 0) {
            return -1;
        }
        if (prev >= 0 && faults > prev && *jump == 0) {
            *jump = frames + 1;
        }
        prev = faults;
    }
    return 0;
}

void k_thrash_show(FILE *out, const char *algo, const int *pages, int n) {
    int high;
    int prev = -1;
    int jump = 0;
    int frames;
    if (out == NULL) {
        return;
    }
    high = distinct_of(pages, n);
    if (high > 6) {
        high = 6;
    }
    fprintf(out, "thrash algo=%s refs=%d\n", algo, n);
    for (frames = high; frames >= 1; frames--) {
        int faults = faults_at(algo, pages, n, frames);
        fprintf(out, "frames=%d faults=%d rate=%d/%d\n", frames, faults, faults, n);
        if (prev >= 0 && faults > prev && jump == 0) {
            jump = frames + 1;
        }
        prev = faults;
    }
    fprintf(out, "jump-below=%d\n", jump);
}

typedef struct {
    int q[8];
    int n;
    int faults;
} FifoQ;

static int q_has(const FifoQ *s, int page) {
    int i;
    for (i = 0; i < s->n; i++) {
        if (s->q[i] == page) {
            return 1;
        }
    }
    return 0;
}

static void fifo_push(FifoQ *s, int page, int cap) {
    int i;
    if (q_has(s, page)) {
        return;
    }
    s->faults++;
    if (s->n < cap) {
        s->q[s->n++] = page;
        return;
    }
    for (i = 1; i < s->n; i++) {
        s->q[i - 1] = s->q[i];
    }
    s->q[s->n - 1] = page;
}

static int walk_belady(int *seq, int len, int target, FifoQ lo, FifoQ hi) {
    int page;
    if (len == target) {
        return lo.faults < hi.faults;
    }
    for (page = 1; page <= 5; page++) {
        FifoQ a = lo;
        FifoQ b = hi;
        int remain = target - (len + 1);
        seq[len] = page;
        fifo_push(&a, page, 3);
        fifo_push(&b, page, 4);
        if (a.faults >= b.faults + remain) {
            continue;
        }
        if (walk_belady(seq, len + 1, target, a, b)) {
            return 1;
        }
    }
    return 0;
}

static int pair_faults(const KBelady *found, int id, int *lo, int *hi) {
    KAccess refs[12];
    KReport report;
    int frames;
    int i;
    for (i = 0; i < found->len; i++) {
        refs[i].page = found->refs[i];
        refs[i].write = 0;
    }
    for (frames = 3; frames <= 4; frames++) {
        if (k_repl_run(k_repl_name(id), frames, NULL, 0, refs, found->len, &report) != 0) {
            return 0;
        }
        if (frames == 3) {
            *lo = report.faults;
        } else {
            *hi = report.faults;
        }
    }
    return 1;
}

int k_belady(KBelady *out) {
    int seq[12];
    int len;
    FifoQ blank;
    int id;
    if (out == NULL) {
        return -1;
    }
    memset(&blank, 0, sizeof blank);
    memset(out, 0, sizeof *out);
    for (len = 8; len <= 12; len++) {
        if (!walk_belady(seq, 0, len, blank, blank)) {
            continue;
        }
        out->len = len;
        memcpy(out->refs, seq, (size_t)len * sizeof(int));
        for (id = 0; id < 5; id++) {
            if (!pair_faults(out, id, &out->faults[id][0], &out->faults[id][1])) {
                return -1;
            }
        }
        return 0;
    }
    return -1;
}

void k_belady_show(FILE *out, const KBelady *found) {
    const char *tag[] = {"fifo-up", "lru-down", "opt-down", "clock-up", "clock2"};
    const int expect_up[] = {1, 0, 0, 1, -1};
    int i;
    int id;
    if (out == NULL || found == NULL) {
        return;
    }
    fprintf(out, "search alphabet=1..5 len=8..12 frames=3,4 order=lex\nfound len=%d refs=", found->len);
    for (i = 0; i < found->len; i++) {
        if (i) {
            fputc(' ', out);
        }
        fprintf(out, "%d", found->refs[i]);
    }
    fputc('\n', out);
    for (id = 0; id < 5; id++) {
        fprintf(out, "%s frames=3 faults=%d\n", k_repl_name(id), found->faults[id][0]);
        fprintf(out, "%s frames=4 faults=%d\n", k_repl_name(id), found->faults[id][1]);
    }
    fputs("note=", out);
    for (id = 0; id < 5; id++) {
        int up = found->faults[id][0] < found->faults[id][1];
        if (id) {
            fputc(' ', out);
        }
        if (expect_up[id] < 0) {
            fprintf(out, "%s=%s", k_repl_name(id), up ? "up" : "down");
        } else if (up == expect_up[id]) {
            fputs(tag[id], out);
        } else {
            fprintf(out, "%s-mismatch", k_repl_name(id));
        }
    }
    fputc('\n', out);
}
