#include "cli.h"

#include "balance.h"
#include "best.h"
#include "exam.h"
#include "ext.h"
#include "loser.h"
#include "repl.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int parse_num(const char *text, long long *out, long long lo, long long hi,
                     const char **err) {
    char *end = NULL;
    long long value;
    errno = 0;
    if (text == NULL || *text == '\0') {
        *err = "format";
        return 0;
    }
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        *err = "format";
        return 0;
    }
    if (value < lo || value > hi) {
        *err = "range";
        return 0;
    }
    *out = value;
    return 1;
}

static void cmd_passes(int argc, char **argv) {
    long long runs;
    long long ways;
    long long records;
    int pass;
    long long io;
    const char *err = "format";
    if (argc != 4 || !parse_num(argv[1], &runs, 1, BAL_RUN_MAX, &err) ||
        !parse_num(argv[2], &ways, 2, BAL_WAY_MAX, &err) ||
        !parse_num(argv[3], &records, 0, BAL_REC_MAX, &err)) {
        fail(err);
        return;
    }
    pass = merge_passes(runs, (int)ways);
    io = balance_io(records, pass);
    if (pass < 0 || io < 0) {
        fail("range");
        return;
    }
    printf("passes=%d\n", pass);
    printf("io=%lld\n", io);
}

static void cmd_plain(int argc, char **argv) {
    long long records;
    long long memory;
    long long runs;
    const char *err = "format";
    if (argc != 3 || !parse_num(argv[1], &records, 1, BAL_REC_MAX, &err) ||
        !parse_num(argv[2], &memory, 1, BAL_REC_MAX, &err)) {
        fail(err);
        return;
    }
    runs = plain_runs(records, memory);
    if (runs < 0) {
        fail("range");
        return;
    }
    printf("runs=%lld\n", runs);
}

static void cmd_dummy(int argc, char **argv) {
    long long runs;
    long long ways;
    int dummy;
    const char *err = "format";
    if (argc != 3 || !parse_num(argv[1], &runs, 1, BAL_RUN_MAX, &err) ||
        !parse_num(argv[2], &ways, 2, BAL_WAY_MAX, &err)) {
        fail(err);
        return;
    }
    dummy = dummy_count(runs, (int)ways);
    if (dummy < 0) {
        fail("range");
        return;
    }
    printf("dummy=%d\n", dummy);
}

static void cmd_kcmp(int argc, char **argv) {
    long long records;
    long long ways;
    SelectBound bound;
    const char *err = "format";
    if (argc != 3 || !parse_num(argv[1], &records, 1, BAL_REC_MAX, &err) ||
        !parse_num(argv[2], &ways, 2, BAL_WAY_MAX, &err)) {
        fail(err);
        return;
    }
    if (select_bounds(records, (int)ways, &bound) != 0) {
        fail("range");
        return;
    }
    printf("path=%d\n", bound.path);
    printf("formula=%lld\n", bound.formula);
    printf("naive_rest=%lld\n", bound.naive_rest);
    printf("naive_all=%lld\n", bound.naive_all);
}

static void cmd_best(int argc, char **argv) {
    long long ways;
    long long len[BEST_RUNS];
    int n;
    int i;
    BestTree tree;
    const char *err = "format";
    if (argc < 3 || !parse_num(argv[1], &ways, 2, BEST_WAYS, &err)) {
        fail(err);
        return;
    }
    n = argc - 2;
    if (n > BEST_RUNS) {
        fail("range");
        return;
    }
    for (i = 0; i < n; i++) {
        if (!parse_num(argv[i + 2], &len[i], 1, BEST_LEN_MAX, &err)) {
            fail(err);
            return;
        }
    }
    if (best_tree((int)ways, len, n, &tree) != 0) {
        fail("range");
        return;
    }
    best_print(&tree, stdout);
}

static int nondecreasing(const int *key, int n) {
    int i;
    for (i = 1; i < n; i++) {
        if (key[i] < key[i - 1]) {
            return 0;
        }
    }
    return 1;
}

static int parse_lose(int argc, char **argv, LoseIn *in, const char **err) {
    int seg = 0;
    int len = 0;
    int i;
    memset(in, 0, sizeof(*in));
    if (argc < 2) {
        *err = "format";
        return 0;
    }
    for (i = 1; i < argc; i++) {
        long long key;
        if (strcmp(argv[i], "/") == 0) {
            if (len == 0 || seg >= LOSER_K - 1) {
                *err = len == 0 ? "format" : "range";
                return 0;
            }
            if (!nondecreasing(in->data[seg], len)) {
                *err = "range";
                return 0;
            }
            in->len[seg++] = len;
            len = 0;
            continue;
        }
        if (len >= LOSER_SEG || !parse_num(argv[i], &key, EXT_KEY_MIN, EXT_KEY_MAX, err)) {
            if (len >= LOSER_SEG) {
                *err = "range";
            }
            return 0;
        }
        in->data[seg][len++] = (int)key;
    }
    if (len == 0 || !nondecreasing(in->data[seg], len)) {
        *err = len == 0 ? "format" : "range";
        return 0;
    }
    in->len[seg++] = len;
    in->k = seg;
    if (in->k < 2) {
        *err = "range";
        return 0;
    }
    return 1;
}

static void cmd_lose(int argc, char **argv) {
    LoseIn in;
    LoseResult result;
    const char *err = "format";
    if (!parse_lose(argc, argv, &in, &err)) {
        fail(err);
        return;
    }
    if (loser_merge(&in, &result) != 0) {
        fail("range");
        return;
    }
    loser_print(&result, stdout);
}

static void cmd_repl(int argc, char **argv) {
    long long memory;
    int key[REPL_N];
    int n;
    int i;
    Repl result;
    const char *err = "format";
    if (argc < 3 || !parse_num(argv[1], &memory, 1, REPL_M, &err)) {
        fail(err);
        return;
    }
    n = argc - 2;
    if (n > REPL_N) {
        fail("range");
        return;
    }
    for (i = 0; i < n; i++) {
        long long value;
        if (!parse_num(argv[i + 2], &value, EXT_KEY_MIN, EXT_KEY_MAX, &err)) {
            fail(err);
            return;
        }
        key[i] = (int)value;
    }
    if (repl_select((int)memory, key, n, &result) != 0) {
        fail("range");
        return;
    }
    repl_print(&result, stdout);
}

static void cmd_card(int argc) {
    if (argc != 1) {
        fail("format");
        return;
    }
    printf("time=io\n");
    printf("more_k=passes\n");
    printf("loser=compare\n");
    printf("repl=runs\n");
    printf("best=io\n");
}

static void cmd_exam(int argc, char **argv) {
    if (argc != 2) {
        fail("format");
        return;
    }
    if (exam_run(argv[1]) != 0) {
        fail("unknown");
    }
}

void cli_dispatch(int argc, char **argv) {
    if (strcmp(argv[0], "passes") == 0) {
        cmd_passes(argc, argv);
    } else if (strcmp(argv[0], "plain") == 0) {
        cmd_plain(argc, argv);
    } else if (strcmp(argv[0], "dummy") == 0) {
        cmd_dummy(argc, argv);
    } else if (strcmp(argv[0], "kcmp") == 0) {
        cmd_kcmp(argc, argv);
    } else if (strcmp(argv[0], "best") == 0) {
        cmd_best(argc, argv);
    } else if (strcmp(argv[0], "lose") == 0) {
        cmd_lose(argc, argv);
    } else if (strcmp(argv[0], "repl") == 0) {
        cmd_repl(argc, argv);
    } else if (strcmp(argv[0], "card") == 0) {
        cmd_card(argc);
    } else if (strcmp(argv[0], "exam") == 0) {
        cmd_exam(argc, argv);
    } else {
        fail("unknown");
    }
}
