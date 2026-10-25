#include "cli.h"

#include "avl.h"
#include "bst.h"
#include "exam.h"
#include "node.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_MAX 64
#define KEY_LO (-1000000)
#define KEY_HI 1000000
#define HEIGHT_MAX 12

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int read_int(const char *s, int *out, int lo, int hi) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || value < lo || value > hi) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

/* 0 成功，-1 某个词不是整数，-2 个数超出 KEY_MAX。 */
static int parse_keys(char **argv, int from, int argc, int *keys, int *n) {
    *n = argc - from;
    if (*n > KEY_MAX) {
        return -2;
    }
    for (int i = 0; i < *n; i++) {
        if (!read_int(argv[from + i], &keys[i], KEY_LO, KEY_HI)) {
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

static void print_tree(const char *tag, const Node *t, int with_bf) {
    printf("%s ", tag);
    node_fprint(t, stdout, with_bf);
    fputc('\n', stdout);
}

static void print_dec(const char *name, long long num, long long den) {
    long long scaled = (num * 100 + den / 2) / den;
    printf("%s=%lld/%lld dec=%lld.%02lld\n", name, num, den, scaled / 100, scaled % 100);
}

static Node *insert_keys(const int *keys, int n, int avl) {
    Node *t = NULL;
    for (int i = 0; i < n; i++) {
        int dup = 0;
        t = avl ? avl_insert(t, keys[i], 0, NULL, &dup) : bst_insert(t, keys[i], &dup);
    }
    return t;
}

static void cmd_bst(int argc, char **argv) {
    int keys[KEY_MAX];
    int n = 0;
    int rc = parse_keys(argv, 1, argc, keys, &n);
    Node *t = NULL;
    if (rc || n < 1) {
        report_parse(rc ? rc : -1);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    printf("bst\n");
    for (int i = 0; i < n; i++) {
        int dup = 0;
        printf("insert %d\n", keys[i]);
        t = bst_insert(t, keys[i], &dup);
        print_tree("tree", t, 0);
    }
    printf("inorder ");
    node_inorder(t, stdout);
    fputc('\n', stdout);
    node_free(t);
}

static void cmd_find(int argc, char **argv) {
    int target = 0;
    int keys[KEY_MAX];
    int n = 0;
    int rc;
    Node *t;
    if (argc < 2 || !read_int(argv[1], &target, KEY_LO, KEY_HI)) {
        fail("format");
        return;
    }
    rc = parse_keys(argv, 2, argc, keys, &n);
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 0);
    printf("find %d\n", target);
    printf("%s\n", bst_search(t, target, stdout) ? "hit" : "miss");
    node_free(t);
}

static void cmd_del(int argc, char **argv) {
    int victim = 0;
    int keys[KEY_MAX];
    int n = 0;
    int rc;
    int found = 0;
    Node *t;
    if (argc < 2 || !read_int(argv[1], &victim, KEY_LO, KEY_HI)) {
        fail("format");
        return;
    }
    rc = parse_keys(argv, 2, argc, keys, &n);
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 0);
    printf("del %d\n", victim);
    print_tree("before", t, 0);
    t = bst_delete(t, victim, stdout, &found);
    if (!found) {
        printf("missing\n");
    } else {
        print_tree("after", t, 0);
    }
    node_free(t);
}

static void cmd_back(int argc, char **argv) {
    int victim = 0;
    int keys[KEY_MAX];
    int n = 0;
    int rc;
    int found = 0;
    int dup = 0;
    Node *t;
    Node *orig;
    if (argc < 2 || !read_int(argv[1], &victim, KEY_LO, KEY_HI)) {
        fail("format");
        return;
    }
    rc = parse_keys(argv, 2, argc, keys, &n);
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 0);
    orig = node_clone(t);
    printf("back %d\n", victim);
    print_tree("before", t, 0);
    t = bst_delete(t, victim, stdout, &found);
    if (!found) {
        printf("missing\n");
        node_free(t);
        node_free(orig);
        return;
    }
    print_tree("mid", t, 0);
    t = bst_insert(t, victim, &dup);
    print_tree("after", t, 0);
    printf("same=%s\n", node_equal(orig, t) ? "yes" : "no");
    node_free(t);
    node_free(orig);
}

static void cmd_asl(int argc, char **argv) {
    int keys[KEY_MAX];
    int n = 0;
    int rc = parse_keys(argv, 1, argc, keys, &n);
    Node *t;
    Asl asl;
    if (rc || n < 1) {
        report_parse(rc ? rc : -1);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 0);
    printf("asl\n");
    print_tree("tree", t, 0);
    printf("level ");
    node_levels(t, stdout);
    fputc('\n', stdout);
    bst_asl(t, &asl);
    print_dec("success", asl.succ_num, asl.succ_den);
    print_dec("fail", asl.fail_num, asl.fail_den);
    node_free(t);
}

static int parse_pairs(char **argv, int argc, int *keys, int *weight, int *n) {
    *n = argc - 1;
    if (*n < 1) {
        return -1;
    }
    if (*n > KEY_MAX) {
        return -2;
    }
    for (int i = 0; i < *n; i++) {
        char *colon = strchr(argv[i + 1], ':');
        if (!colon || colon == argv[i + 1] || colon[1] == '\0') {
            return -1;
        }
        *colon = '\0';
        if (!read_int(argv[i + 1], &keys[i], KEY_LO, KEY_HI) ||
            !read_int(colon + 1, &weight[i], 1, 1000000)) {
            return -1;
        }
    }
    return 0;
}

static void cmd_aslw(int argc, char **argv) {
    int keys[KEY_MAX];
    int weight[KEY_MAX];
    int n = 0;
    int rc = parse_pairs(argv, argc, keys, weight, &n);
    Node *t;
    long long num = 0;
    long long den = 0;
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 0);
    printf("aslw\n");
    print_tree("tree", t, 0);
    printf("level ");
    node_levels(t, stdout);
    fputc('\n', stdout);
    printf("weight");
    for (int i = 0; i < n; i++) {
        printf(" %d=%d", keys[i], weight[i]);
    }
    fputc('\n', stdout);
    bst_asl_weight(t, keys, weight, n, &num, &den);
    print_dec("success", num, den);
    node_free(t);
}

static void cmd_avl(int argc, char **argv) {
    int keys[KEY_MAX];
    int n = 0;
    int rc = parse_keys(argv, 1, argc, keys, &n);
    Node *t = NULL;
    if (rc || n < 1) {
        report_parse(rc ? rc : -1);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    printf("avl\n");
    for (int i = 0; i < n; i++) {
        int dup = 0;
        printf("insert %d\n", keys[i]);
        t = avl_insert(t, keys[i], 0, stdout, &dup);
        print_tree("tree", t, 1);
    }
    node_free(t);
}

static void cmd_avldel(int argc, char **argv) {
    int victim = 0;
    int keys[KEY_MAX];
    int n = 0;
    int rc;
    int found = 0;
    Node *t;
    if (argc < 2 || !read_int(argv[1], &victim, KEY_LO, KEY_HI)) {
        fail("format");
        return;
    }
    rc = parse_keys(argv, 2, argc, keys, &n);
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 1);
    printf("avldel %d\n", victim);
    print_tree("before", t, 1);
    t = avl_delete(t, victim, stdout, &found);
    if (!found) {
        printf("missing\n");
    } else {
        print_tree("after", t, 1);
    }
    node_free(t);
}

static void cmd_avlback(int argc, char **argv) {
    int victim = 0;
    int keys[KEY_MAX];
    int n = 0;
    int rc;
    int found = 0;
    int dup = 0;
    Node *t;
    Node *orig;
    if (argc < 2 || !read_int(argv[1], &victim, KEY_LO, KEY_HI)) {
        fail("format");
        return;
    }
    rc = parse_keys(argv, 2, argc, keys, &n);
    if (rc) {
        report_parse(rc);
        return;
    }
    if (has_dup(keys, n)) {
        fail("dup");
        return;
    }
    t = insert_keys(keys, n, 1);
    orig = node_clone(t);
    printf("avlback %d\n", victim);
    print_tree("before", orig, 1);
    t = avl_delete(t, victim, stdout, &found);
    if (!found) {
        printf("missing\n");
        node_free(t);
        node_free(orig);
        return;
    }
    print_tree("mid", t, 1);
    printf("insert %d\n", victim);
    t = avl_insert(t, victim, 0, stdout, &dup);
    print_tree("after", t, 1);
    printf("same=%s\n", node_equal(orig, t) ? "yes" : "no");
    node_free(t);
    node_free(orig);
}

static void cmd_min(int argc, char **argv) {
    int h = 0;
    Node *t;
    if (argc != 2 || !read_int(argv[1], &h, 1, HEIGHT_MAX)) {
        /* 词不是整数算格式；高度不在 1..12 算范围。 */
        if (argc == 2) {
            char *end = NULL;
            errno = 0;
            strtol(argv[1], &end, 10);
            fail(end != argv[1] && *end == '\0' && errno == 0 ? "range" : "format");
        } else {
            fail("format");
        }
        return;
    }
    printf("min h=%d\n", h);
    avl_min_report(stdout, h);
    if (h <= 4) {
        t = avl_build_min(h);
        print_tree("tree", t, 1);
        node_free(t);
    }
}

static void cmd_exam(int argc, char **argv) {
    if (argc != 2) {
        fail("format");
        return;
    }
    if (exam_run(argv[1], stdout) < 0) {
        printf("fail unknown exam %s\n", argv[1]);
    }
}

void cli_dispatch(int argc, char **argv) {
    if (strcmp(argv[0], "bst") == 0) {
        cmd_bst(argc, argv);
    } else if (strcmp(argv[0], "find") == 0) {
        cmd_find(argc, argv);
    } else if (strcmp(argv[0], "del") == 0) {
        cmd_del(argc, argv);
    } else if (strcmp(argv[0], "back") == 0) {
        cmd_back(argc, argv);
    } else if (strcmp(argv[0], "asl") == 0) {
        cmd_asl(argc, argv);
    } else if (strcmp(argv[0], "aslw") == 0) {
        cmd_aslw(argc, argv);
    } else if (strcmp(argv[0], "avl") == 0) {
        cmd_avl(argc, argv);
    } else if (strcmp(argv[0], "avldel") == 0) {
        cmd_avldel(argc, argv);
    } else if (strcmp(argv[0], "avlback") == 0) {
        cmd_avlback(argc, argv);
    } else if (strcmp(argv[0], "min") == 0) {
        cmd_min(argc, argv);
    } else if (strcmp(argv[0], "exam") == 0) {
        cmd_exam(argc, argv);
    } else {
        printf("fail unknown %s\n", argv[0]);
    }
}
