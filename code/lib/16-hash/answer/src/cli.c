#include "cli.h"

#include "hash.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HashTab *cur;
static int bias;

void cli_end(void) {
    hash_free(cur);
    cur = NULL;
}

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int read_i(const char *s, long long lo, long long hi, int *out, const char **err) {
    char *end = NULL;
    long long value;
    errno = 0;
    value = strtoll(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE) {
        *err = "format";
        return 0;
    }
    if (value < lo || value > hi) {
        *err = "range";
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int need_table(void) {
    if (cur == NULL) {
        fail("empty");
        return 0;
    }
    return 1;
}

static void insert_all(HashTab *t, const int *keys, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (t->kind == KIND_CHAIN) {
            ChainWalk w;
            int rc = chain_insert(t, keys[i], &w);
            show_chain_ins(keys[i], &w);
            if (rc != HASH_OK) {
                return;
            }
        } else {
            OpenWalk w;
            int rc = open_insert(t, keys[i], &w);
            show_open_ins(keys[i], &w);
            if (rc != HASH_OK) {
                return;
            }
        }
    }
}

static int read_keys(char **argv, int from, int n, int *keys, const char **err) {
    int i;
    for (i = 0; i < n; i++) {
        if (!read_i(argv[from + i], 0, HASH_KEY_MAX, &keys[i], err)) {
            return 0;
        }
    }
    return 1;
}

static int take_build(int kind, int argc, char **argv, int *m, int *p, int *a, int *q, int *keys,
                      int *nkeys) {
    const char *err = "format";
    int base = kind == KIND_DOUBLE ? 5 : 4;
    *q = 0;
    if (argc < base) {
        fail("format");
        return 0;
    }
    if (!read_i(argv[1], 1, HASH_M_MAX, m, &err) || !read_i(argv[2], 1, HASH_M_MAX, p, &err) ||
        !read_i(argv[3], 1, HASH_A_MAX, a, &err)) {
        fail(err);
        return 0;
    }
    if (*p > *m) {
        fail("range");
        return 0;
    }
    if (kind == KIND_DOUBLE) {
        if (!read_i(argv[4], 1, HASH_M_MAX, q, &err)) {
            fail(err);
            return 0;
        }
        if (*q >= *m) {
            fail("range");
            return 0;
        }
    }
    *nkeys = argc - base;
    if (*nkeys > HASH_N_MAX || !read_keys(argv, base, *nkeys, keys, &err)) {
        fail(*nkeys > HASH_N_MAX ? "range" : err);
        return 0;
    }
    return 1;
}

static void cmd_build(int kind, int argc, char **argv) {
    int m = 0;
    int p = 0;
    int a = 0;
    int q = 0;
    int nkeys = 0;
    int keys[HASH_N_MAX];
    HashTab *t;
    if (!take_build(kind, argc, argv, &m, &p, &a, &q, keys, &nkeys)) {
        return;
    }
    t = hash_create(kind, m, p, a, q);
    if (t == NULL) {
        fail("range");
        return;
    }
    hash_free(cur);
    cur = t;
    t->b = bias;
    insert_all(t, keys, nkeys);
    show_summary(t);
}

static int one_key(int argc, char **argv, int *key) {
    const char *err = "format";
    if (!need_table()) {
        return 0;
    }
    if (argc != 2 || !read_i(argv[1], 0, HASH_KEY_MAX, key, &err)) {
        fail(argc != 2 ? "format" : err);
        return 0;
    }
    return 1;
}

static void cmd_find(const char *cmd, int argc, char **argv) {
    int key = 0;
    if (!one_key(argc, argv, &key)) {
        return;
    }
    if (cur->kind == KIND_CHAIN) {
        ChainWalk w;
        chain_find(cur, key, &w);
        show_chain_query(cmd, key, &w);
        return;
    }
    OpenWalk w;
    open_find(cur, key, &w);
    show_open_query(cmd, key, &w);
}

static void cmd_del(int wipe, int argc, char **argv) {
    int key = 0;
    if (!one_key(argc, argv, &key)) {
        return;
    }
    if (wipe && cur->kind == KIND_CHAIN) {
        fail("kind");
        return;
    }
    if (cur->kind == KIND_CHAIN) {
        ChainWalk w;
        chain_delete(cur, key, &w);
        show_chain_del(key, &w);
        return;
    }
    OpenWalk w;
    open_delete(cur, key, wipe, &w);
    show_open_del(wipe ? "erase" : "del", key, &w, wipe);
}

static void cmd_bias(int argc, char **argv) {
    const char *err = "format";
    int value = 0;
    if (argc != 2 || !read_i(argv[1], 0, HASH_A_MAX, &value, &err)) {
        fail(argc == 2 ? err : "format");
        return;
    }
    bias = value;
    printf("bias=%d\n", bias);
}

static void cmd_asl(int argc, char **argv) {
    int extra = 0;
    if (!need_table()) {
        return;
    }
    if (argc == 2 && strcmp(argv[1], "null") == 0) {
        extra = 1;
    } else if (argc != 1) {
        fail("format");
        return;
    }
    show_asl(cur, extra);
}

static void cmd_runs(int argc) {
    int run;
    if (argc != 1) {
        fail("format");
        return;
    }
    if (!need_table()) {
        return;
    }
    run = hash_run(cur);
    if (run < 0) {
        fail("kind");
        return;
    }
    printf("run=%d\n", run);
}

static void cmd_theory(int argc, char **argv) {
    const char *err = "format";
    int kind;
    int n = 0;
    int m = 0;
    if (argc != 4) {
        fail("format");
        return;
    }
    kind = hash_kind_from(argv[1]);
    if (kind == KIND_NONE) {
        fail("format");
        return;
    }
    if (!read_i(argv[2], 0, HASH_THEORY_MAX, &n, &err) ||
        !read_i(argv[3], 1, HASH_THEORY_MAX, &m, &err)) {
        fail(err);
        return;
    }
    show_formula(kind, n, m);
}

static void cmd_grow(int argc, char **argv) {
    const char *err = "format";
    int kind;
    int p = 0;
    int n = 0;
    int m = 0;
    if (argc != 5) {
        fail("format");
        return;
    }
    kind = hash_kind_from(argv[1]);
    if (kind == KIND_NONE) {
        fail("format");
        return;
    }
    if (kind == KIND_DOUBLE) {
        fail("kind");
        return;
    }
    if (!read_i(argv[2], 1, HASH_M_MAX, &p, &err) || !read_i(argv[3], 1, HASH_N_MAX, &n, &err) ||
        !read_i(argv[4], 1, HASH_M_MAX, &m, &err)) {
        fail(err);
        return;
    }
    if (p > m) {
        fail("range");
        return;
    }
    fact_grow(kind, p, n, m);
}

static void match_letter(long long num, long long den, int opt[][2]) {
    int i;
    long long g = hash_gcd(num, den);
    printf("value=%lld/%lld\n", num / g, den / g);
    for (i = 0; i < 4; i++) {
        if (num * opt[i][1] == den * (long long)opt[i][0]) {
            printf("letter=%c\n", 'A' + i);
            return;
        }
    }
    printf("letter=none\n");
}

static int read_opts(char **argv, int opt[][2]) {
    const char *err = "format";
    int i;
    for (i = 0; i < 4; i++) {
        int num = 0;
        int den = 0;
        if (!read_i(argv[2 + 2 * i], 0, HASH_THEORY_MAX, &num, &err) ||
            !read_i(argv[3 + 2 * i], 1, HASH_THEORY_MAX, &den, &err)) {
            fail(err);
            return 0;
        }
        opt[i][0] = num;
        opt[i][1] = den;
    }
    return 1;
}

static void cmd_match(int argc, char **argv) {
    int opt[4][2];
    int use_fail;
    HashAsl asl;
    if (!need_table()) {
        return;
    }
    if (argc != 10) {
        fail("format");
        return;
    }
    if (strcmp(argv[1], "success") == 0) {
        use_fail = 0;
    } else if (strcmp(argv[1], "fail") == 0) {
        use_fail = 1;
    } else {
        fail("format");
        return;
    }
    if (!read_opts(argv, opt)) {
        return;
    }
    hash_asl(cur, 0, &asl);
    if (use_fail) {
        if (asl.depend) {
            fail("kind");
            return;
        }
        match_letter(asl.fail_num, asl.fail_den, opt);
        return;
    }
    if (!asl.has_succ) {
        fail("empty");
        return;
    }
    match_letter(asl.succ_num, asl.succ_den, opt);
}

static void cmd_plain(int argc, void (*fn)(void)) {
    if (argc != 1) {
        fail("format");
        return;
    }
    fn();
}

void cli_dispatch(int argc, char **argv) {
    int kind = hash_kind_from(argv[0]);
    if (kind != KIND_NONE) {
        cmd_build(kind, argc, argv);
        return;
    }
    if (strcmp(argv[0], "find") == 0 || strcmp(argv[0], "miss") == 0) {
        cmd_find(argv[0], argc, argv);
    } else if (strcmp(argv[0], "del") == 0) {
        cmd_del(0, argc, argv);
    } else if (strcmp(argv[0], "erase") == 0) {
        cmd_del(1, argc, argv);
    } else if (strcmp(argv[0], "bias") == 0) {
        cmd_bias(argc, argv);
    } else if (strcmp(argv[0], "asl") == 0) {
        cmd_asl(argc, argv);
    } else if (strcmp(argv[0], "runs") == 0) {
        cmd_runs(argc);
    } else if (strcmp(argv[0], "show") == 0) {
        if (argc != 1) {
            fail("format");
        } else if (need_table()) {
            show_summary(cur);
        }
    } else if (strcmp(argv[0], "theory") == 0) {
        cmd_theory(argc, argv);
    } else if (strcmp(argv[0], "grow") == 0) {
        cmd_grow(argc, argv);
    } else if (strcmp(argv[0], "match") == 0) {
        cmd_match(argc, argv);
    } else if (strcmp(argv[0], "clash") == 0) {
        cmd_plain(argc, fact_clash);
    } else if (strcmp(argv[0], "improve") == 0) {
        cmd_plain(argc, fact_improve);
    } else {
        fail("unknown");
    }
}
