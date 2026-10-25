#include "cli.h"

#include "exam.h"
#include "heap.h"
#include "huff.h"
#include "topk.h"
#include "ufset.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EDGE_MAX 48

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

static int kind_of(const char *s) {
    if (strcmp(s, "max") == 0) {
        return 1;
    }
    if (strcmp(s, "min") == 0) {
        return 0;
    }
    return -1;
}

static int read_keys(char **argv, int from, int to, int *keys, int *n, int lo, int hi) {
    *n = to - from;
    if (*n < 1 || *n > HEAP_MAX) {
        return 0;
    }
    for (int i = 0; i < *n; i++) {
        if (!read_int(argv[from + i], &keys[i], lo, hi)) {
            return 0;
        }
    }
    return 1;
}

static int bits_only(const char *s, int maxlen) {
    int n = (int)strlen(s);
    if (n < 1 || n > maxlen) {
        return 0;
    }
    for (const char *p = s; *p != '\0'; p++) {
        if (*p != '0' && *p != '1') {
            return 0;
        }
    }
    return 1;
}

static void cmd_build(int argc, char **argv) {
    int keys[HEAP_MAX];
    int n = 0;
    int kind;
    Heap h;
    if (argc < 3 || (kind = kind_of(argv[1])) < 0 ||
        !read_keys(argv, 2, argc, keys, &n, -1000000, 1000000)) {
        fail("format");
        return;
    }
    heap_build(&h, keys, n, kind, stdout);
}

static void cmd_insert(int argc, char **argv) {
    int keys[HEAP_MAX];
    int n = 0;
    int kind;
    Heap h;
    if (argc < 3 || (kind = kind_of(argv[1])) < 0 ||
        !read_keys(argv, 2, argc, keys, &n, -1000000, 1000000)) {
        fail("format");
        return;
    }
    heap_insert_all(&h, keys, n, kind, stdout);
}

static int find_plus(int argc, char **argv) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "+") == 0) {
            return i;
        }
    }
    return -1;
}

static void cmd_push(int argc, char **argv) {
    int keys[HEAP_MAX];
    int extra[HEAP_MAX];
    int n = 0;
    int m = 0;
    int kind;
    int plus;
    Heap h;
    plus = find_plus(argc, argv);
    if (argc < 5 || (kind = kind_of(argv[1])) < 0 || plus < 3 || plus == argc - 1) {
        fail("format");
        return;
    }
    if (!read_keys(argv, 2, plus, keys, &n, -1000000, 1000000) ||
        !read_keys(argv, plus + 1, argc, extra, &m, -1000000, 1000000)) {
        fail("format");
        return;
    }
    if (heap_load(&h, keys, n, kind) != HEAP_OK || !heap_ok(&h)) {
        fail("heap");
        return;
    }
    fprintf(stdout, "push %s\n", kind ? "max" : "min");
    for (int i = 0; i < m; i++) {
        heap_push(&h, extra[i], stdout);
    }
}

static void cmd_pop(int argc, char **argv) {
    int keys[HEAP_MAX];
    int n = 0;
    int times = 0;
    int kind;
    Heap h;
    if (argc < 4 || !read_int(argv[1], &times, 1, HEAP_MAX) || (kind = kind_of(argv[2])) < 0 ||
        !read_keys(argv, 3, argc, keys, &n, -1000000, 1000000)) {
        fail("format");
        return;
    }
    if (heap_load(&h, keys, n, kind) != HEAP_OK || times > n) {
        fail("range");
        return;
    }
    if (!heap_ok(&h)) {
        fail("heap");
        return;
    }
    fprintf(stdout, "pop %s\n", kind ? "max" : "min");
    for (int i = 0; i < times; i++) {
        int got = 0;
        heap_pop(&h, &got, stdout);
    }
}

static void cmd_topk(int argc, char **argv) {
    int keys[HEAP_MAX];
    int n = 0;
    int k = 0;
    if (argc < 3 || !read_int(argv[1], &k, 1, HEAP_MAX) ||
        !read_keys(argv, 2, argc, keys, &n, -1000000, 1000000) ||
        topk_run(keys, n, k, stdout) != HEAP_OK) {
        fail("format");
    }
}

static int take_leaf(const char *s, char *name, int *weight) {
    const char *colon = strchr(s, ':');
    if (colon == NULL) {
        *name = 0;
        return read_int(s, weight, 1, 1000000);
    }
    if (colon != s + 1 || !isalpha((unsigned char)s[0])) {
        return 0;
    }
    *name = s[0];
    return read_int(colon + 1, weight, 1, 1000000);
}

static int dup_name(const char *names, int n, char name) {
    for (int i = 0; i < n; i++) {
        if (names[i] == name) {
            return 1;
        }
    }
    return 0;
}

static void cmd_huff(int argc, char **argv) {
    char names[HUFF_MAX + 1];
    int weight[HUFF_MAX];
    int named = 0;
    int plain = 0;
    Huff tree;
    if (argc < 2 || argc - 1 > HUFF_MAX) {
        fail("format");
        return;
    }
    for (int i = 1; i < argc; i++) {
        char name = 0;
        if (!take_leaf(argv[i], &name, &weight[i - 1])) {
            fail("format");
            return;
        }
        if (name == 0) {
            plain++;
            names[i - 1] = (char)('A' + i - 1);
        } else {
            named++;
            if (dup_name(names, i - 1, name)) {
                fail("dup");
                return;
            }
            names[i - 1] = name;
        }
    }
    if (named > 0 && plain > 0) {
        fail("format");
        return;
    }
    names[argc - 1] = '\0';
    huff_build(&tree, names, weight, argc - 1, stdout);
}

static void cmd_prefix(int argc, char **argv) {
    char codes[HUFF_MAX][HUFF_CODE];
    char bad_a[HUFF_CODE];
    char bad_b[HUFF_CODE];
    int n = argc - 1;
    if (n < 1 || n > HUFF_MAX) {
        fail("format");
        return;
    }
    for (int i = 0; i < n; i++) {
        if (!bits_only(argv[i + 1], HUFF_CODE - 1)) {
            fail("format");
            return;
        }
        snprintf(codes[i], HUFF_CODE, "%s", argv[i + 1]);
    }
    if (code_prefix(codes, n, bad_a, bad_b) == HUFF_OK) {
        printf("prefix=ok\n");
        return;
    }
    printf("prefix=bad %s %s\n", bad_a, bad_b);
}

static int take_code(const char *s, char *name, char *code) {
    const char *colon = strchr(s, ':');
    if (colon == NULL || colon != s + 1 || !isalpha((unsigned char)s[0]) ||
        !bits_only(colon + 1, HUFF_CODE - 1)) {
        return 0;
    }
    *name = s[0];
    snprintf(code, HUFF_CODE, "%s", colon + 1);
    return 1;
}

static void cmd_decode(int argc, char **argv) {
    char codes[HUFF_MAX][HUFF_CODE];
    char names[HUFF_MAX + 1];
    char text[HUFF_MAX * 4];
    int n = argc - 2;
    int rc;
    if (argc < 3 || n > HUFF_MAX || !bits_only(argv[1], 128)) {
        fail("format");
        return;
    }
    for (int i = 0; i < n; i++) {
        if (!take_code(argv[i + 2], &names[i], codes[i])) {
            fail("format");
            return;
        }
        if (dup_name(names, i, names[i])) {
            fail("dup");
            return;
        }
    }
    names[n] = '\0';
    rc = code_decode(argv[1], names, codes, n, text, (int)sizeof text);
    if (rc == HUFF_PREFIX) {
        fail("prefix");
        return;
    }
    if (rc != HUFF_OK) {
        fail("decode");
        return;
    }
    printf("text=%s\n", text);
}

static int mode_of(const char *s) {
    if (strcmp(s, "plain") == 0) {
        return UF_PLAIN;
    }
    if (strcmp(s, "rank") == 0) {
        return UF_RANK;
    }
    if (strcmp(s, "both") == 0) {
        return UF_BOTH;
    }
    return -1;
}

static int parse_edge(const char *s, int *u, int *v, int n) {
    char left[16];
    char right[16];
    const char *dash = strchr(s, '-');
    int nleft;
    if (dash == NULL || dash == s || strchr(dash + 1, '-') != NULL) {
        return 0;
    }
    nleft = (int)(dash - s);
    if (nleft <= 0 || nleft >= (int)sizeof left || strlen(dash + 1) >= sizeof right) {
        return 0;
    }
    memcpy(left, s, (size_t)nleft);
    left[nleft] = '\0';
    memcpy(right, dash + 1, strlen(dash + 1) + 1);
    return read_int(left, u, 1, n) && read_int(right, v, 1, n);
}

static void cmd_uf(int argc, char **argv) {
    int mode;
    int n = 0;
    Uf set;
    if (argc < 3 || (mode = mode_of(argv[1])) < 0 || !read_int(argv[2], &n, 1, UF_MAX)) {
        fail("format");
        return;
    }
    for (int i = 3; i < argc; i++) {
        int a = 0;
        int b = 0;
        if (strcmp(argv[i], "find") == 0) {
            if (i + 1 >= argc || !read_int(argv[i + 1], &a, 1, n)) {
                fail("format");
                return;
            }
            i++;
            continue;
        }
        if (!parse_edge(argv[i], &a, &b, n)) {
            fail("format");
            return;
        }
    }
    uf_begin(&set, n, mode, stdout);
    for (int i = 3; i < argc; i++) {
        int a = 0;
        int b = 0;
        if (strcmp(argv[i], "find") == 0) {
            read_int(argv[++i], &a, 1, n);
            uf_find(&set, a, stdout);
            continue;
        }
        parse_edge(argv[i], &a, &b, n);
        uf_union(&set, a, b, stdout);
    }
}

static void cmd_cycle(int argc, char **argv) {
    int edge_a[EDGE_MAX];
    int edge_b[EDGE_MAX];
    int n = 0;
    int m = 0;
    if (argc < 2 || !read_int(argv[1], &n, 1, UF_MAX) || argc - 2 > EDGE_MAX) {
        fail("format");
        return;
    }
    for (int i = 2; i < argc; i++) {
        if (!parse_edge(argv[i], &edge_a[m], &edge_b[m], n)) {
            fail("format");
            return;
        }
        m++;
    }
    uf_cycle(n, edge_a, edge_b, m, stdout);
}

static void cmd_paths(int argc, char **argv) {
    int left[8];
    int right[8];
    int slash = -1;
    int na;
    int nb;
    if (argc < 6) {
        fail("format");
        return;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/") == 0) {
            if (slash >= 0) {
                fail("format");
                return;
            }
            slash = i;
        }
    }
    if (slash < 2 || slash == argc - 1) {
        fail("format");
        return;
    }
    na = slash - 1;
    nb = argc - slash - 1;
    if (na < 2 || nb < 2 || na > 8 || nb > 8) {
        fail("range");
        return;
    }
    for (int i = 0; i < na; i++) {
        if (!read_int(argv[i + 1], &left[i], 1, 1000000)) {
            fail("format");
            return;
        }
    }
    for (int i = 0; i < nb; i++) {
        if (!read_int(argv[slash + 1 + i], &right[i], 1, 1000000)) {
            fail("format");
            return;
        }
    }
    printf("same=%s\n", huff_paths_same(left, na, right, nb) ? "yes" : "no");
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
    if (strcmp(argv[0], "build") == 0) {
        cmd_build(argc, argv);
    } else if (strcmp(argv[0], "insert") == 0) {
        cmd_insert(argc, argv);
    } else if (strcmp(argv[0], "push") == 0) {
        cmd_push(argc, argv);
    } else if (strcmp(argv[0], "pop") == 0) {
        cmd_pop(argc, argv);
    } else if (strcmp(argv[0], "topk") == 0) {
        cmd_topk(argc, argv);
    } else if (strcmp(argv[0], "huff") == 0) {
        cmd_huff(argc, argv);
    } else if (strcmp(argv[0], "prefix") == 0) {
        cmd_prefix(argc, argv);
    } else if (strcmp(argv[0], "decode") == 0) {
        cmd_decode(argc, argv);
    } else if (strcmp(argv[0], "uf") == 0) {
        cmd_uf(argc, argv);
    } else if (strcmp(argv[0], "cycle") == 0) {
        cmd_cycle(argc, argv);
    } else if (strcmp(argv[0], "paths") == 0) {
        cmd_paths(argc, argv);
    } else if (strcmp(argv[0], "exam") == 0) {
        cmd_exam(argc, argv);
    } else {
        printf("fail unknown %s\n", argv[0]);
    }
}
