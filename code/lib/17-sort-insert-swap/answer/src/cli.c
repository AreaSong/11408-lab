#include "cli.h"

#include "bubble.h"
#include "insert.h"
#include "judge.h"
#include "quick.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gaps[SORT_MAX];
static int ngap;

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int parse_int(const char *text, int *out) {
    char *end;
    long value;
    if (text == NULL || *text == '\0') {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (*end != '\0' || value < KEY_MIN || value > KEY_MAX) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int load_keys(int argc, char **argv, int from, Seq *s) {
    int key[SORT_MAX];
    int n = argc - from;
    int i;
    if (n < 1 || n > SORT_MAX) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (!parse_int(argv[from + i], &key[i])) {
            return 0;
        }
    }
    seq_load(s, key, n);
    return 1;
}

static int find_eq(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "=") == 0) {
            return i;
        }
    }
    return -1;
}

static void cmd_gap(int argc, char **argv) {
    int i;
    if (argc == 1) {
        ngap = 0;
        printf("gap=half\n");
        return;
    }
    if (argc - 1 > SORT_MAX) {
        fail("range");
        return;
    }
    for (i = 1; i < argc; i++) {
        if (!parse_int(argv[i], &gaps[i - 1]) || gaps[i - 1] < 1) {
            fail("range");
            return;
        }
    }
    ngap = argc - 1;
    printf("gap");
    for (i = 0; i < ngap; i++) {
        printf(" %d", gaps[i]);
    }
    putchar('\n');
}

static void cmd_shell(int argc, char **argv) {
    Seq s;
    int local[SORT_MAX];
    const int *use = gaps;
    int nuse = ngap;
    int i;
    if (!load_keys(argc, argv, 1, &s)) {
        fail("range");
        return;
    }
    if (nuse == 0) {
        nuse = shell_halve(s.n, local);
        use = local;
    }
    for (i = 0; i < nuse; i++) {
        if (use[i] < 1 || use[i] >= s.n) {
            fail("range");
            return;
        }
    }
    shell_trace(&s, use, nuse);
}

static void cmd_trace(int argc, char **argv, int kind) {
    Seq s;
    if (!load_keys(argc, argv, 1, &s)) {
        fail("range");
        return;
    }
    if (kind == 0) {
        insert_trace(&s, 0);
    } else if (kind == 1) {
        insert_trace(&s, 1);
    } else if (kind == 2) {
        bubble_trace(&s, 0);
    } else if (kind == 3) {
        bubble_trace(&s, 1);
    } else {
        quick_trace(&s);
    }
}

static void cmd_fit(int argc, char **argv) {
    Seq s;
    int pass;
    if (argc < 3 || !parse_int(argv[1], &pass) || !load_keys(argc, argv, 2, &s)) {
        fail("format");
        return;
    }
    if (pass < 1 || pass > s.n) {
        fail("range");
        return;
    }
    judge_fit(&s, pass);
}

static void cmd_qopt(int argc, char **argv) {
    Seq opt[4];
    int key[SORT_MAX];
    int pass;
    int n;
    int i;
    int t;
    if (argc < 3 || !parse_int(argv[1], &pass) || !parse_int(argv[2], &n)) {
        fail("format");
        return;
    }
    if (pass < 1 || n < 1 || n > SORT_MAX || argc != 3 + 4 * n) {
        fail("range");
        return;
    }
    for (i = 0; i < 4; i++) {
        int j;
        for (j = 0; j < n; j++) {
            if (!parse_int(argv[3 + i * n + j], &key[j])) {
                fail("format");
                return;
            }
        }
        seq_load(&opt[i], key, n);
    }
    t = pass;
    judge_qopt(opt, t);
}

static int load_eq(int argc, char **argv, int *left, int *nleft, Seq *right) {
    int eq = find_eq(argc, argv);
    int i;
    if (eq < 0 || !load_keys(argc, argv, eq + 1, right)) {
        return 0;
    }
    *nleft = eq - 1;
    if (*nleft < 1 || *nleft > 4) {
        return 0;
    }
    for (i = 0; i < *nleft; i++) {
        if (!parse_int(argv[1 + i], &left[i])) {
            return 0;
        }
    }
    return 1;
}

static void cmd_dorder(int argc, char **argv) {
    Seq s;
    int gap[4];
    int ng = 0;
    if (!load_eq(argc, argv, gap, &ng, &s) || ng != 4) {
        fail("format");
        return;
    }
    judge_dorder(&s, gap, ng);
}

static void cmd_placed(int argc, char **argv) {
    Seq s;
    int choice[4];
    int nc = 0;
    if (!load_eq(argc, argv, choice, &nc, &s) || nc != 4) {
        fail("format");
        return;
    }
    judge_placed(&s, choice);
}

static void cmd_shell2(int argc, char **argv) {
    Seq init;
    Seq p1;
    Seq p2;
    int pair[8];
    int raw[SORT_MAX * 3];
    int n;
    int i;
    if (argc < 2 || !parse_int(argv[1], &n) || n < 2 || n > SORT_MAX) {
        fail("range");
        return;
    }
    if (argc != 2 + 3 * n + 8) {
        fail("format");
        return;
    }
    for (i = 0; i < 3 * n; i++) {
        if (!parse_int(argv[2 + i], &raw[i])) {
            fail("format");
            return;
        }
    }
    for (i = 0; i < 8; i++) {
        if (!parse_int(argv[2 + 3 * n + i], &pair[i]) || pair[i] < 1 || pair[i] >= n) {
            fail("range");
            return;
        }
    }
    seq_load(&init, raw, n);
    seq_load(&p1, raw + n, n);
    seq_load(&p2, raw + 2 * n, n);
    judge_shell2(&init, &p1, &p2, pair);
}

static int read_family(int argc, char **argv, Seq *seg, int *nseg) {
    int buf[6][SORT_MAX];
    int len[6];
    int n = 0;
    int count = 0;
    int i;
    for (i = 2; i <= argc; i++) {
        int boundary = i == argc || strcmp(argv[i], "/") == 0;
        if (!boundary) {
            if (n >= 6 || count >= SORT_MAX || !parse_int(argv[i], &buf[n][count])) {
                return 0;
            }
            count++;
            continue;
        }
        if (count == 0 || n >= 6) {
            return 0;
        }
        len[n++] = count;
        count = 0;
    }
    if (n < 2) {
        return 0;
    }
    for (i = 1; i < n; i++) {
        if (len[i] != len[0]) {
            return 0;
        }
    }
    for (i = 0; i < n; i++) {
        seq_load(&seg[i], buf[i], len[i]);
    }
    *nseg = n;
    return 1;
}

static void cmd_family(int argc, char **argv) {
    Seq seg[6];
    int nseg = 0;
    int year;
    if (argc < 2 || !parse_int(argv[1], &year) || (year != 2010 && year != 2025)) {
        fail("format");
        return;
    }
    if (!read_family(argc, argv, seg, &nseg) || nseg < 2 || nseg > 5) {
        fail("format");
        return;
    }
    judge_family(year, &seg[0], &seg[1], nseg - 1);
}

static void cmd_half(int argc, char **argv) {
    Seq s;
    if (!load_keys(argc, argv, 1, &s)) {
        fail("range");
        return;
    }
    judge_half(&s);
}

void cli_dispatch(int argc, char **argv) {
    if (strcmp(argv[0], "gap") == 0) {
        cmd_gap(argc, argv);
    } else if (strcmp(argv[0], "insert") == 0) {
        cmd_trace(argc, argv, 0);
    } else if (strcmp(argv[0], "binary") == 0) {
        cmd_trace(argc, argv, 1);
    } else if (strcmp(argv[0], "rise") == 0) {
        cmd_trace(argc, argv, 2);
    } else if (strcmp(argv[0], "sink") == 0) {
        cmd_trace(argc, argv, 3);
    } else if (strcmp(argv[0], "quick") == 0) {
        cmd_trace(argc, argv, 4);
    } else if (strcmp(argv[0], "shell") == 0) {
        cmd_shell(argc, argv);
    } else if (strcmp(argv[0], "fit") == 0) {
        cmd_fit(argc, argv);
    } else if (strcmp(argv[0], "qopt") == 0) {
        cmd_qopt(argc, argv);
    } else if (strcmp(argv[0], "dorder") == 0) {
        cmd_dorder(argc, argv);
    } else if (strcmp(argv[0], "placed") == 0) {
        cmd_placed(argc, argv);
    } else if (strcmp(argv[0], "shell2") == 0) {
        cmd_shell2(argc, argv);
    } else if (strcmp(argv[0], "family") == 0) {
        cmd_family(argc, argv);
    } else if (strcmp(argv[0], "half") == 0) {
        cmd_half(argc, argv);
    } else if (strcmp(argv[0], "calls") == 0 && argc == 1) {
        judge_calls();
    } else if (strcmp(argv[0], "fixes") == 0 && argc == 1) {
        judge_fixes();
    } else if (strcmp(argv[0], "blocks") == 0 && argc == 1) {
        judge_blocks();
    } else if (strcmp(argv[0], "stable") == 0 && argc == 1) {
        judge_stable();
    } else if (strcmp(argv[0], "calls") == 0 || strcmp(argv[0], "fixes") == 0 ||
               strcmp(argv[0], "blocks") == 0 || strcmp(argv[0], "stable") == 0) {
        fail("format");
    } else {
        fail("unknown");
    }
}

void cli_end(void) {
}
