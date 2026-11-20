#include "cli.h"

#include "cost.h"
#include "exam.h"
#include "hsort.h"
#include "merge.h"
#include "meter.h"
#include "radix.h"
#include "select.h"
#include "shape.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int parse_key(const char *text, int *out, const char **err) {
    char *end;
    long value;
    if (text == NULL || *text == '\0') {
        *err = "format";
        return 0;
    }
    value = strtol(text, &end, 10);
    if (*end != '\0') {
        *err = "format";
        return 0;
    }
    if (value < KEY_MIN || value > KEY_MAX) {
        *err = "range";
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int load_from(int argc, char **argv, int from, Seq *s, const char **err) {
    int key[SORT_MAX];
    int n = argc - from;
    int i;
    if (n < 1) {
        *err = "format";
        return 0;
    }
    if (n > SORT_MAX) {
        *err = "range";
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!parse_key(argv[from + i], &key[i], err)) {
            return 0;
        }
    }
    seq_load(s, key, n);
    return 1;
}

static void cmd_select(int argc, char **argv) {
    Seq s;
    const char *err = "format";
    if (!load_from(argc, argv, 1, &s, &err)) {
        fail(err);
        return;
    }
    select_sort(&s, 0, 1);
}

static void cmd_heap(int argc, char **argv) {
    Seq s;
    const char *err = "format";
    if (!load_from(argc, argv, 1, &s, &err)) {
        fail(err);
        return;
    }
    hsort_sort(&s, 0, 1);
}

static void cmd_merge(int argc, char **argv) {
    Seq s;
    const char *err = "format";
    if (!load_from(argc, argv, 1, &s, &err)) {
        fail(err);
        return;
    }
    merge_sort(&s, 0, 1);
}

static void cmd_radix(int argc, char **argv) {
    Seq s;
    const char *err = "format";
    if (!load_from(argc, argv, 1, &s, &err)) {
        fail(err);
        return;
    }
    if (radix_sort(&s, 0, 1) != 0) {
        fail("range");
    }
}

static void cmd_fit(int argc, char **argv) {
    Seq s;
    const char *err = "format";
    int pass;
    if (argc < 3 || !parse_key(argv[1], &pass, &err) || !load_from(argc, argv, 2, &s, &err)) {
        fail(err);
        return;
    }
    if (pass < 1 || pass > s.n) {
        fail("range");
        return;
    }
    shape_report(&s, pass);
}

static void cmd_runs(int argc, char **argv) {
    int flat[SORT_MAX];
    int len[SORT_MAX];
    int nr = 0;
    int total = 0;
    int i;
    long cmp = 0;
    const char *err = "format";
    if (argc < 2) {
        fail("format");
        return;
    }
    len[0] = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/") == 0) {
            if (len[nr] == 0 || nr + 1 >= SORT_MAX) {
                fail(len[nr] == 0 ? "format" : "range");
                return;
            }
            nr++;
            len[nr] = 0;
            continue;
        }
        if (total >= SORT_MAX || !parse_key(argv[i], &flat[total], &err)) {
            fail(total >= SORT_MAX ? "range" : err);
            return;
        }
        total++;
        len[nr]++;
    }
    if (len[nr] == 0) {
        fail("format");
        return;
    }
    if (merge_chain(flat, len, nr + 1, NULL, &cmp, 1) < 0) {
        fail("range");
    }
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
    if (strcmp(argv[0], "select") == 0) {
        cmd_select(argc, argv);
    } else if (strcmp(argv[0], "heap") == 0) {
        cmd_heap(argc, argv);
    } else if (strcmp(argv[0], "merge") == 0) {
        cmd_merge(argc, argv);
    } else if (strcmp(argv[0], "radix") == 0) {
        cmd_radix(argc, argv);
    } else if (strcmp(argv[0], "fit") == 0) {
        cmd_fit(argc, argv);
    } else if (strcmp(argv[0], "runs") == 0) {
        cmd_runs(argc, argv);
    } else if (strcmp(argv[0], "stable") == 0 && argc == 1) {
        stable_show();
    } else if (strcmp(argv[0], "count") == 0 && argc == 1) {
        meter_count();
    } else if (strcmp(argv[0], "table") == 0 && argc == 1) {
        meter_table();
    } else if (strcmp(argv[0], "exam") == 0) {
        cmd_exam(argc, argv);
    } else if (strcmp(argv[0], "stable") == 0 || strcmp(argv[0], "count") == 0 ||
               strcmp(argv[0], "table") == 0) {
        fail("format");
    } else {
        fail("unknown");
    }
}
