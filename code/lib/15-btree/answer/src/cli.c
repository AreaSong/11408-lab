#include "cli.h"

#include "bp.h"
#include "bt.h"
#include "calc.h"
#include "demo.h"
#include "limit.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BtTree *btree;
static BpTree *bplus;
static int which;
static BtPolicy policy;

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static void drop_all(void) {
    bt_free(btree);
    bp_free(bplus);
    btree = NULL;
    bplus = NULL;
    which = 0;
}

void cli_end(void) {
    drop_all();
}

static int read_ll(const char *text, long long *out) {
    char *end = NULL;
    long long value;
    errno = 0;
    value = strtoll(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return 0;
    }
    *out = value;
    return 1;
}

static int take_order(const char *text, int *order, const char **err) {
    long long value;
    if (!read_ll(text, &value)) {
        *err = "format";
        return 0;
    }
    if (!limit_order((int)value)) {
        *err = "range";
        return 0;
    }
    *order = (int)value;
    return 1;
}

static int take_key(const char *text, int *key, const char **err) {
    long long value;
    if (!read_ll(text, &value)) {
        *err = "format";
        return 0;
    }
    if (value < BT_KEY_LO || value > BT_KEY_HI) {
        *err = "range";
        return 0;
    }
    *key = (int)value;
    return 1;
}

static int take_keys(char **argv, int from, int argc, int *keys, int *n, const char **err) {
    *n = argc - from;
    if (*n < 1 || *n > 64) {
        *err = *n < 1 ? "format" : "range";
        return 0;
    }
    for (int i = 0; i < *n; i++) {
        if (!take_key(argv[from + i], &keys[i], err)) {
            return 0;
        }
        for (int j = 0; j < i; j++) {
            if (keys[j] == keys[i]) {
                *err = "dup";
                return 0;
            }
        }
    }
    return 1;
}

static void build_bt(int order, const int *keys, int n) {
    drop_all();
    btree = bt_create(order);
    which = 1;
    printf("bt m=%d\n", order);
    for (int i = 0; i < n; i++) {
        printf("ins %d\n", keys[i]);
        bt_insert(btree, keys[i], stdout);
    }
    bt_report(btree, stdout);
}

static void build_bp(int order, const int *keys, int n) {
    drop_all();
    bplus = bp_create(order);
    which = 2;
    printf("plus m=%d\n", order);
    for (int i = 0; i < n; i++) {
        printf("ins %d\n", keys[i]);
        bp_insert(bplus, keys[i], stdout);
    }
    bp_report(bplus, stdout);
}

static void cmd_batch(int argc, char **argv, int plus) {
    int order = 0;
    int n = 0;
    int keys[64];
    const char *err = "format";
    if (argc < 3 || !take_order(argv[1], &order, &err) || !take_keys(argv, 2, argc, keys, &n, &err)) {
        fail(err);
        return;
    }
    if (plus) {
        build_bp(order, keys, n);
    } else {
        build_bt(order, keys, n);
    }
}

static void cmd_cmp(int argc, char **argv) {
    int order = 0;
    int n = 0;
    int keys[64];
    const char *err = "format";
    BtTree *left;
    BpTree *right;
    if (argc < 3 || !take_order(argv[1], &order, &err) || !take_keys(argv, 2, argc, keys, &n, &err)) {
        fail(err);
        return;
    }
    left = bt_create(order);
    right = bp_create(order);
    for (int i = 0; i < n; i++) {
        bt_insert(left, keys[i], NULL);
        bp_insert(right, keys[i], NULL);
    }
    printf("cmp m=%d\n", order);
    printf("btree\n");
    bt_report(left, stdout);
    printf("bplus\n");
    bp_report(right, stdout);
    bt_free(left);
    bp_free(right);
}

static void cmd_load(int argc, char **argv) {
    int order = 0;
    int rc;
    const char *err = "format";
    BtTree *loaded;
    if (argc < 3 || !take_order(argv[1], &order, &err)) {
        fail(err);
        return;
    }
    loaded = bt_create(order);
    rc = bt_load(loaded, argv + 2, argc - 2);
    if (rc) {
        bt_free(loaded);
        fail(rc == BT_FMT ? "format" : "bad");
        return;
    }
    drop_all();
    btree = loaded;
    which = 1;
    printf("load m=%d\n", order);
    bt_report(btree, stdout);
}

static void cmd_policy(int argc, char **argv) {
    if (argc != 3) {
        fail("format");
        return;
    }
    if (strcmp(argv[1], "pred") != 0 && strcmp(argv[1], "succ") != 0) {
        fail("format");
        return;
    }
    if (strcmp(argv[2], "left") != 0 && strcmp(argv[2], "right") != 0) {
        fail("format");
        return;
    }
    policy.use_succ = strcmp(argv[1], "succ") == 0;
    policy.merge_right = strcmp(argv[2], "right") == 0;
    printf("policy %s %s\n", policy.use_succ ? "succ" : "pred", policy.merge_right ? "right" : "left");
}

static void cmd_ins(int argc, char **argv) {
    int key = 0;
    int rc;
    const char *err = "format";
    if (argc != 2 || !take_key(argv[1], &key, &err)) {
        fail(err);
        return;
    }
    if (which == 0) {
        fail("empty");
        return;
    }
    printf("ins %d\n", key);
    if (which == 1) {
        rc = bt_insert(btree, key, stdout);
        if (rc) {
            fail(rc == BT_DUP ? "dup" : "range");
            return;
        }
        bt_report(btree, stdout);
        return;
    }
    rc = bp_insert(bplus, key, stdout);
    if (rc) {
        fail(rc == BP_DUP ? "dup" : "range");
        return;
    }
    bp_report(bplus, stdout);
}

static void cmd_del(int argc, char **argv) {
    int key = 0;
    int rc;
    const char *err = "format";
    if (argc != 2 || !take_key(argv[1], &key, &err)) {
        fail(err);
        return;
    }
    if (which == 2) {
        fail("kind");
        return;
    }
    if (which == 0 || !btree->root) {
        fail("empty");
        return;
    }
    printf("del %d\n", key);
    rc = bt_delete(btree, key, policy, stdout);
    if (rc) {
        fail("miss");
        return;
    }
    bt_report(btree, stdout);
}

static void cmd_find(int argc, char **argv) {
    int key = 0;
    const char *err = "format";
    if (argc != 2 || !take_key(argv[1], &key, &err)) {
        fail(err);
        return;
    }
    if (which == 0 || (which == 1 && !btree->root) || (which == 2 && !bplus->root)) {
        fail("empty");
        return;
    }
    printf("find %d\n", key);
    if (which == 1) {
        bt_find(btree, key, stdout);
    } else {
        bp_find(bplus, key, stdout);
    }
}

static void cmd_show(void) {
    if (which == 1) {
        bt_report(btree, stdout);
    } else if (which == 2) {
        bp_report(bplus, stdout);
    } else {
        fail("empty");
    }
}

void cli_dispatch(int argc, char **argv) {
    if (calc_dispatch(argc, argv)) {
        return;
    }
    if (strcmp(argv[0], "bt") == 0) {
        cmd_batch(argc, argv, 0);
    } else if (strcmp(argv[0], "plus") == 0) {
        cmd_batch(argc, argv, 1);
    } else if (strcmp(argv[0], "cmp") == 0) {
        cmd_cmp(argc, argv);
    } else if (strcmp(argv[0], "load") == 0) {
        cmd_load(argc, argv);
    } else if (strcmp(argv[0], "policy") == 0) {
        cmd_policy(argc, argv);
    } else if (strcmp(argv[0], "ins") == 0) {
        cmd_ins(argc, argv);
    } else if (strcmp(argv[0], "del") == 0) {
        cmd_del(argc, argv);
    } else if (strcmp(argv[0], "find") == 0) {
        cmd_find(argc, argv);
    } else if (strcmp(argv[0], "show") == 0) {
        if (argc != 1) {
            fail("format");
        } else {
            cmd_show();
        }
    } else if (strcmp(argv[0], "stmt") == 0) {
        if (argc != 1) {
            fail("format");
        } else {
            demo_stmt();
        }
    } else if (strcmp(argv[0], "trait") == 0) {
        if (argc != 1) {
            fail("format");
        } else {
            demo_trait();
        }
    } else {
        fail("unknown");
    }
}
