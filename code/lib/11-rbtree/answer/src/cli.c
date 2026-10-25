#include "cli.h"

#include "check.h"
#include "delete.h"
#include "insert.h"
#include "parse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int read_int(const char *s, int *out) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || value < RB_KEY_LO || value > RB_KEY_HI) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int parse_keys(char **argv, int from, int argc, int *keys, int *n) {
    *n = argc - from;
    if (*n > RB_KEY_MAX) {
        return -2;
    }
    for (int i = 0; i < *n; i++) {
        if (!read_int(argv[from + i], &keys[i])) {
            return -1;
        }
    }
    return 0;
}

static int has_dup(const int *keys, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (keys[i] == keys[j]) {
                return 1;
            }
        }
    }
    return 0;
}

static void report_parse(int rc) {
    fail(rc == -2 ? "range" : "format");
}

static void print_tree(const RbTree *t) {
    printf("tree ");
    rb_fprint(t, t->root, stdout);
    fputc('\n', stdout);
}

static int build(RbTree *t, const int *keys, int n) {
    for (int i = 0; i < n; i++) {
        if (rb_insert(t, keys[i], NULL, NULL)) {
            return -1;
        }
    }
    return 0;
}

static void print_ok(const RbTree *t, int spins) {
    RbStats s;
    rb_stats(t, &s);
    if (!s.rb) {
        printf("bad\n");
        return;
    }
    printf("ok bh=%d reds=%d height=%d spins=%d\n", s.bh, s.reds, s.height, spins);
}

static void cmd_rb(const int *keys, int n) {
    RbTree *t = rb_create();
    RbStats s;
    printf("rb\n");
    for (int i = 0; i < n; i++) {
        int spins = 0;
        printf("insert %d\n", keys[i]);
        rb_insert(t, keys[i], stdout, &spins);
        print_tree(t);
        print_ok(t, spins);
    }
    printf("inorder");
    if (n > 0) {
        fputc(' ', stdout);
        rb_print_inorder(t, stdout);
    }
    fputc('\n', stdout);
    rb_stats(t, &s);
    printf("done n=%d reds=%d height=%d bh=%d\n", s.nodes, s.reds, s.height, s.bh);
    rb_free(t);
}

static void print_root(const RbStats *s) {
    if (s->root_state == 0) {
        printf("root=none\n");
    } else if (s->root_state == 1) {
        printf("root=black\n");
    } else {
        printf("root=red\n");
    }
}

static void cmd_judge(const char *shape) {
    RbTree *t = rb_create();
    RbStats s;
    int rc = rb_parse(t, shape);
    if (rc) {
        report_parse(rc);
        rb_free(t);
        return;
    }
    printf("judge %s\n", shape);
    rb_dump(t, stdout);
    rb_stats(t, &s);
    printf("bst=%s\n", s.bst ? "yes" : "no");
    printf("color=%s\n", s.color_ok ? "yes" : "no");
    print_root(&s);
    printf("nil=%s\n", s.nil_black ? "black" : "red");
    printf("redred=%s\n", s.redred ? "yes" : "no");
    if (!s.redred && s.has_pair) {
        printf("pair %d %d\n", s.pair_parent, s.pair_child);
    }
    printf("bh=%s\n", s.bh_ok ? "yes" : "no");
    if (s.bh_ok) {
        printf("bh_value=%d\n", s.bh);
    }
    printf("rb=%s\n", s.rb ? "yes" : "no");
    rb_free(t);
}

static void cmd_del(int key, const int *keys, int n) {
    RbTree *t = rb_create();
    int spins = 0;
    printf("del %d\n", key);
    build(t, keys, n);
    printf("before ");
    rb_fprint(t, t->root, stdout);
    fputc('\n', stdout);
    if (rb_delete(t, key, stdout, &spins)) {
        printf("missing\n");
        rb_free(t);
        return;
    }
    printf("after ");
    rb_fprint(t, t->root, stdout);
    fputc('\n', stdout);
    print_ok(t, spins);
    rb_free(t);
}

static void cmd_bound(const int *keys, int n) {
    RbTree *t = rb_create();
    RbStats s;
    long long two_h;
    long long cap;
    printf("bound\n");
    build(t, keys, n);
    print_tree(t);
    rb_stats(t, &s);
    two_h = 1LL << s.height;
    cap = (long long)(s.nodes + 1) * (s.nodes + 1);
    printf("n=%d\n", s.nodes);
    printf("height=%d\n", s.height);
    printf("bh=%d\n", s.bh);
    printf("long=%d\n", s.long_path);
    printf("short=%d\n", s.short_path);
    printf("twice=%s\n", s.long_path <= 2 * s.short_path ? "yes" : "no");
    printf("two_h=%lld\n", two_h);
    printf("cap=%lld\n", cap);
    printf("lt=%s\n", two_h < cap ? "yes" : "no");
    printf("le=%s\n", two_h <= cap ? "yes" : "no");
    printf("reds=%d\n", s.reds);
    rb_free(t);
}

static int load_keys(char **argv, int from, int argc, int *keys, int *n) {
    int rc = parse_keys(argv, from, argc, keys, n);
    if (rc) {
        report_parse(rc);
        return 0;
    }
    if (has_dup(keys, *n)) {
        fail("dup");
        return 0;
    }
    return 1;
}

void cli_dispatch(int argc, char **argv) {
    int keys[RB_KEY_MAX];
    int n = 0;
    int key = 0;
    if (strcmp(argv[0], "rb") == 0) {
        if (!load_keys(argv, 1, argc, keys, &n)) {
            return;
        }
        cmd_rb(keys, n);
        return;
    }
    if (strcmp(argv[0], "judge") == 0) {
        if (argc != 2) {
            fail("format");
            return;
        }
        cmd_judge(argv[1]);
        return;
    }
    if (strcmp(argv[0], "del") == 0) {
        if (argc < 2 || !read_int(argv[1], &key)) {
            fail("format");
            return;
        }
        if (!load_keys(argv, 2, argc, keys, &n)) {
            return;
        }
        cmd_del(key, keys, n);
        return;
    }
    if (strcmp(argv[0], "bound") == 0) {
        if (!load_keys(argv, 1, argc, keys, &n)) {
            return;
        }
        cmd_bound(keys, n);
        return;
    }
    fail("unknown");
}
