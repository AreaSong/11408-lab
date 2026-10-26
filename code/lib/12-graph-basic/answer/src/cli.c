#include "cli.h"

#include "cross.h"
#include "enumdfs.h"
#include "fact.h"
#include "graph.h"
#include "multi.h"
#include "walk.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Graph g;
static int ready;

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int read_int(const char *s, int *out) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || value < -1000000 || value > 1000000) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int need_graph(void) {
    if (!ready) {
        fail("empty");
        return 0;
    }
    return 1;
}

static int exact_args(int argc, int want) {
    if (argc != want) {
        fail("format");
        return 0;
    }
    return 1;
}

static int parse_one(int argc, char **argv, int *n) {
    if (!exact_args(argc, 2)) {
        return 0;
    }
    if (!read_int(argv[1], n)) {
        fail("format");
        return 0;
    }
    return 1;
}

static int parse_src(int argc, char **argv, int *src) {
    if (!need_graph() || !parse_one(argc, argv, src)) {
        return 0;
    }
    if (*src < 0 || *src >= g.n) {
        fail("range");
        return 0;
    }
    return 1;
}

static void cmd_new(int argc, char **argv) {
    GKind kind;
    int n = 0;
    if (!exact_args(argc, 3)) {
        return;
    }
    if (strcmp(argv[1], "U") == 0) {
        kind = G_U;
    } else if (strcmp(argv[1], "D") == 0) {
        kind = G_D;
    } else {
        fail("format");
        return;
    }
    if (!read_int(argv[2], &n)) {
        fail("format");
        return;
    }
    if (n < 1 || n > G_MAX) {
        fail("range");
        return;
    }
    graph_init(&g, kind, n);
    ready = 1;
    printf("kind=%c n=%d e=0\n", kind == G_U ? 'U' : 'D', n);
}

static void cmd_edge(int argc, char **argv) {
    int u = 0;
    int v = 0;
    int rc;
    if (!need_graph() || !exact_args(argc, 3)) {
        return;
    }
    if (!read_int(argv[1], &u) || !read_int(argv[2], &v)) {
        fail("format");
        return;
    }
    rc = graph_add(&g, u, v);
    if (rc == ADD_LOOP) {
        fail("loop");
    } else if (rc == ADD_DUP) {
        fail("dup");
    } else if (rc == ADD_RANGE) {
        fail("range");
    } else if (rc == ADD_MEM) {
        fail("nomem");
    }
}

static void cmd_show(int argc, char **argv) {
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    graph_show(&g, stdout);
}

static void cmd_canon(int argc, char **argv) {
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    graph_canon(&g);
    graph_show(&g, stdout);
}

static void cmd_deg(int argc, char **argv) {
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    graph_deg(&g, stdout);
}

static void cmd_space(int argc, char **argv) {
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    graph_space(&g, stdout);
}

static void cmd_cross(int argc, char **argv) {
    int rc;
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    rc = cross_print(&g, stdout);
    if (rc == -1) {
        fail("kind");
    } else if (rc != 0) {
        fail("nomem");
    }
}

static void cmd_multi(int argc, char **argv) {
    int rc;
    (void)argv;
    if (!exact_args(argc, 1) || !need_graph()) {
        return;
    }
    rc = multi_print(&g, stdout);
    if (rc == -1) {
        fail("kind");
    } else if (rc != 0) {
        fail("nomem");
    }
}

static void cmd_bfs(int argc, char **argv) {
    int src = 0;
    if (parse_src(argc, argv, &src)) {
        walk_bfs(&g, src, stdout);
    }
}

static void cmd_bfs_all(int argc, char **argv) {
    (void)argv;
    if (exact_args(argc, 1) && need_graph()) {
        walk_bfs_all(&g, stdout);
    }
}

static void cmd_dfs(int argc, char **argv) {
    int src = 0;
    if (parse_src(argc, argv, &src)) {
        walk_dfs(&g, src, stdout);
    }
}

static void cmd_dfs_all(int argc, char **argv) {
    (void)argv;
    if (exact_args(argc, 1) && need_graph()) {
        walk_dfs_all(&g, stdout);
    }
}

static void cmd_dfs_stack(int argc, char **argv) {
    int src = 0;
    if (parse_src(argc, argv, &src)) {
        walk_dfs_stack(&g, src, stdout);
    }
}

static void cmd_dfs_flip(int argc, char **argv) {
    int src = 0;
    if (parse_src(argc, argv, &src)) {
        walk_dfs_flip(&g, src, stdout);
    }
}

static void cmd_reach(int argc, char **argv) {
    (void)argv;
    if (exact_args(argc, 1) && need_graph()) {
        walk_reach(&g, stdout);
    }
}

static void cmd_enum_dfs(int argc, char **argv) {
    int src = 0;
    int rc;
    if (!parse_src(argc, argv, &src)) {
        return;
    }
    rc = enum_dfs(&g, src, stdout);
    if (rc == -1) {
        fail("enum");
    } else if (rc != 0) {
        fail("nomem");
    }
}

static void cmd_bound(int argc, char **argv) {
    int n = 0;
    if (!parse_one(argc, argv, &n)) {
        return;
    }
    if (fact_bound(n, stdout) != 0) {
        fail("range");
    }
}

static void cmd_bound_check(int argc, char **argv) {
    int n = 0;
    if (!parse_one(argc, argv, &n)) {
        return;
    }
    if (fact_bound_check(n, stdout) != 0) {
        fail("range");
    }
}

static void cmd_enum(int argc, char **argv) {
    int n = 0;
    if (!parse_one(argc, argv, &n)) {
        return;
    }
    if (fact_enum(n, stdout) != 0) {
        fail("range");
    }
}

static void cmd_least(int argc, char **argv) {
    int edges = 0;
    int below = 0;
    int deg[8];
    int cnt[8];
    int np = 0;
    LeastIn in;
    if (argc < 3 || ((argc - 3) % 2) != 0) {
        fail("format");
        return;
    }
    if (!read_int(argv[1], &edges) || !read_int(argv[2], &below)) {
        fail("format");
        return;
    }
    if (edges < 0 || edges > 120 || below < 0 || below > 15) {
        fail("range");
        return;
    }
    for (int i = 3; i < argc; i += 2) {
        if (np >= 8 || !read_int(argv[i], &deg[np]) || !read_int(argv[i + 1], &cnt[np])) {
            fail(np >= 8 ? "range" : "format");
            return;
        }
        if (deg[np] < 0 || deg[np] > 15 || cnt[np] < 1 || cnt[np] > 16) {
            fail("range");
            return;
        }
        np++;
    }
    in.edges = edges;
    in.below = below;
    in.deg = deg;
    in.cnt = cnt;
    in.npairs = np;
    fact_least(&in, stdout);
}

void cli_dispatch(int argc, char **argv) {
    const char *cmd = argv[0];
    if (strcmp(cmd, "new") == 0) {
        cmd_new(argc, argv);
    } else if (strcmp(cmd, "edge") == 0) {
        cmd_edge(argc, argv);
    } else if (strcmp(cmd, "show") == 0) {
        cmd_show(argc, argv);
    } else if (strcmp(cmd, "canon") == 0) {
        cmd_canon(argc, argv);
    } else if (strcmp(cmd, "deg") == 0) {
        cmd_deg(argc, argv);
    } else if (strcmp(cmd, "space") == 0) {
        cmd_space(argc, argv);
    } else if (strcmp(cmd, "cross") == 0) {
        cmd_cross(argc, argv);
    } else if (strcmp(cmd, "multi") == 0) {
        cmd_multi(argc, argv);
    } else if (strcmp(cmd, "bfs") == 0) {
        cmd_bfs(argc, argv);
    } else if (strcmp(cmd, "bfs-all") == 0) {
        cmd_bfs_all(argc, argv);
    } else if (strcmp(cmd, "dfs") == 0) {
        cmd_dfs(argc, argv);
    } else if (strcmp(cmd, "dfs-all") == 0) {
        cmd_dfs_all(argc, argv);
    } else if (strcmp(cmd, "dfs-stack") == 0) {
        cmd_dfs_stack(argc, argv);
    } else if (strcmp(cmd, "dfs-flip") == 0) {
        cmd_dfs_flip(argc, argv);
    } else if (strcmp(cmd, "reach") == 0) {
        cmd_reach(argc, argv);
    } else if (strcmp(cmd, "enum-dfs") == 0) {
        cmd_enum_dfs(argc, argv);
    } else if (strcmp(cmd, "bound") == 0) {
        cmd_bound(argc, argv);
    } else if (strcmp(cmd, "bound-check") == 0) {
        cmd_bound_check(argc, argv);
    } else if (strcmp(cmd, "enum") == 0) {
        cmd_enum(argc, argv);
    } else if (strcmp(cmd, "least") == 0) {
        cmd_least(argc, argv);
    } else {
        fail("unknown");
    }
}
