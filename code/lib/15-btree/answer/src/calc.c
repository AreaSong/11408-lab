#include "calc.h"

#include "limit.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *what) {
    printf("fail %s\n", what);
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

static int read_span(const char *text, long long lo, long long hi, int *out, const char **err) {
    long long value;
    if (!read_ll(text, &value)) {
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

static void letter_of(long long value, const long long *opt) {
    for (int i = 0; i < 4; i++) {
        if (opt[i] == value) {
            printf("letter=%c\n", 'A' + i);
            return;
        }
    }
    printf("letter=none\n");
}

static int read_opts(char **argv, int from, long long *opt) {
    for (int i = 0; i < 4; i++) {
        if (!read_ll(argv[from + i], &opt[i])) {
            return 0;
        }
    }
    return 1;
}

static void show_bounds(int m, int h) {
    long long min_k = 0;
    long long max_k = 0;
    long long min_n = 0;
    long long max_n = 0;
    if (!limit_bounds(m, h, &min_k, &max_k, &min_n, &max_n)) {
        fail("range");
        return;
    }
    printf("bound m=%d h=%d\n", m, h);
    printf("min_keys=%lld max_keys=%lld\n", min_k, max_k);
    printf("min_nodes=%lld max_nodes=%lld\n", min_n, max_n);
}

static void show_choice(const char *mode, long long value, const long long *opt) {
    printf("value=%lld\n", value);
    letter_of(value, opt);
    (void)mode;
}

static int choice_bound(int argc, char **argv) {
    long long opt[4];
    long long min_k, max_k, min_n, max_n, value;
    int m = 0;
    int h = 0;
    const char *err = "format";
    if (argc != 8 || !read_span(argv[2], 3, 16, &m, &err) || !read_span(argv[3], 1, 12, &h, &err) ||
        !read_opts(argv, 4, opt) || !limit_bounds(m, h, &min_k, &max_k, &min_n, &max_n)) {
        fail(err);
        return 1;
    }
    value = strcmp(argv[1], "min") == 0 ? min_k : max_k;
    printf("choice %s m=%d h=%d\n", argv[1], m, h);
    show_choice(argv[1], value, opt);
    return 1;
}

static int choice_nodes(int argc, char **argv) {
    long long opt[4];
    long long value = 0;
    int n = 0;
    int m = 0;
    const char *err = "format";
    if (argc != 8 || !read_span(argv[2], 1, 64, &n, &err) || !read_span(argv[3], 3, 16, &m, &err) ||
        !read_opts(argv, 4, opt) || !limit_max_nodes(n, m, &value)) {
        fail(err);
        return 1;
    }
    printf("choice nodes n=%d m=%d\n", n, m);
    show_choice("nodes", value, opt);
    return 1;
}

static int choice_most(int argc, char **argv) {
    long long opt[4];
    long long value = 0;
    int m = 0;
    int h = 0;
    int keys = 0;
    const char *err = "format";
    if (argc != 9 || !read_span(argv[2], 3, 16, &m, &err) || !read_span(argv[3], 2, 12, &h, &err) ||
        !read_span(argv[4], 1, 10000, &keys, &err) || !read_opts(argv, 5, opt) ||
        !limit_most(m, h, keys, &value)) {
        fail(err);
        return 1;
    }
    printf("choice most m=%d h=%d level2=%d\n", m, h, keys);
    show_choice("most", value, opt);
    return 1;
}

static int choice_shapes(int argc, char **argv) {
    long long opt[4];
    long long value = 0;
    int n = 0;
    int m = 0;
    const char *err = "format";
    if (argc != 8 || !read_span(argv[2], 1, 12, &n, &err) || !read_span(argv[3], 3, 16, &m, &err) ||
        !read_opts(argv, 4, opt) || !limit_shapes(n, m, &value)) {
        fail(err);
        return 1;
    }
    printf("choice shapes n=%d m=%d\n", n, m);
    show_choice("shapes", value, opt);
    return 1;
}

static int cmd_choice(int argc, char **argv) {
    if (argc < 2) {
        fail("format");
        return 1;
    }
    if (strcmp(argv[1], "min") == 0 || strcmp(argv[1], "max") == 0) {
        return choice_bound(argc, argv);
    }
    if (strcmp(argv[1], "nodes") == 0) {
        return choice_nodes(argc, argv);
    }
    if (strcmp(argv[1], "most") == 0) {
        return choice_most(argc, argv);
    }
    if (strcmp(argv[1], "shapes") == 0) {
        return choice_shapes(argc, argv);
    }
    fail("format");
    return 1;
}

int calc_dispatch(int argc, char **argv) {
    int m = 0;
    int h = 0;
    int n = 0;
    int keys = 0;
    long long value = 0;
    const char *err = "format";
    if (strcmp(argv[0], "choice") == 0) {
        return cmd_choice(argc, argv);
    }
    if (strcmp(argv[0], "bound") == 0) {
        if (argc != 3 || !read_span(argv[1], 3, 16, &m, &err) || !read_span(argv[2], 1, 12, &h, &err)) {
            fail(err);
            return 1;
        }
        show_bounds(m, h);
        return 1;
    }
    if (strcmp(argv[0], "nodes") == 0) {
        if (argc != 3 || !read_span(argv[1], 1, 64, &n, &err) || !read_span(argv[2], 3, 16, &m, &err) ||
            !limit_max_nodes(n, m, &value)) {
            fail(err);
            return 1;
        }
        printf("nodes n=%d m=%d\n", n, m);
        printf("max_nodes=%lld\n", value);
        return 1;
    }
    if (strcmp(argv[0], "most") == 0) {
        if (argc != 4 || !read_span(argv[1], 3, 16, &m, &err) || !read_span(argv[2], 2, 12, &h, &err) ||
            !read_span(argv[3], 1, 10000, &keys, &err) || !limit_most(m, h, keys, &value)) {
            fail(err);
            return 1;
        }
        printf("most m=%d h=%d level2=%d\n", m, h, keys);
        printf("max_nodes=%lld\n", value);
        return 1;
    }
    if (strcmp(argv[0], "shapes") == 0) {
        if (argc != 3 || !read_span(argv[1], 1, 12, &n, &err) || !read_span(argv[2], 3, 16, &m, &err) ||
            !limit_shapes(n, m, &value)) {
            fail(err);
            return 1;
        }
        printf("shapes n=%d m=%d\n", n, m);
        printf("count=%lld\n", value);
        return 1;
    }
    return 0;
}
