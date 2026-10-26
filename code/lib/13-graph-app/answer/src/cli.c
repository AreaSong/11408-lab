#include "cli.h"

#include "aoe.h"
#include "graph.h"
#include "mst.h"
#include "path.h"
#include "topo.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Graph cur;
static int alive;

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int parse_int(const char *s, int *out) {
    char *end;
    long v;
    if (s == NULL || *s == '\0') {
        return 0;
    }
    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE) {
        return 0;
    }
    if (v < -2000000000L || v > 2000000000L) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int need_graph(void) {
    if (!alive) {
        fail("empty");
        return 0;
    }
    return 1;
}

static void say(int rc) {
    if (rc == G_KIND) {
        fail("kind");
    } else if (rc == G_RANGE) {
        fail("range");
    } else if (rc == G_HEAP) {
        fail("heap");
    } else if (rc == G_NEG) {
        fail("neg");
    } else if (rc == G_MANY) {
        fail("many");
    }
}

static void cmd_new(int argc, char **argv) {
    int n = 0;
    int kind;
    if (argc != 3 || (strcmp(argv[1], "U") != 0 && strcmp(argv[1], "D") != 0)) {
        fail("format");
        return;
    }
    if (!parse_int(argv[2], &n) || n < 1 || n > G_NMAX) {
        fail("range");
        return;
    }
    kind = argv[1][0] == 'U' ? G_U : G_D;
    graph_init(&cur, kind, n);
    alive = 1;
    printf("kind=%c n=%d e=0\n", argv[1][0], n);
}

static void cmd_edge(int argc, char **argv) {
    int u = 0;
    int v = 0;
    int w = 0;
    int rc;
    if (!need_graph()) {
        return;
    }
    if (argc != 4 || !parse_int(argv[1], &u) || !parse_int(argv[2], &v) || !parse_int(argv[3], &w)) {
        fail("format");
        return;
    }
    rc = graph_add(&cur, u, v, w);
    if (rc == G_LOOP) {
        fail("loop");
    } else if (rc == G_DUP) {
        fail("dup");
    } else if (rc != G_OK) {
        fail("range");
    }
}

static void cmd_show(int argc) {
    if (argc != 1) {
        fail("format");
        return;
    }
    if (!need_graph()) {
        return;
    }
    graph_show(&cur, stdout);
}

static void cmd_prim(int argc, char **argv) {
    Mst mst;
    int start = 0;
    if (!need_graph()) {
        return;
    }
    if (argc != 2 || !parse_int(argv[1], &start)) {
        fail("format");
        return;
    }
    say(prim_run(&cur, start, &mst, stdout));
}

static void cmd_kruskal(int argc) {
    Mst mst;
    if (!need_graph()) {
        return;
    }
    if (argc != 1) {
        fail("format");
        return;
    }
    say(kruskal_run(&cur, &mst, stdout));
}

static void cmd_dijkstra(int argc, char **argv) {
    Sp sp;
    int src = 0;
    if (!need_graph()) {
        return;
    }
    if (argc != 2 || !parse_int(argv[1], &src)) {
        fail("format");
        return;
    }
    say(dijkstra_run(&cur, src, &sp, stdout));
}

static void cmd_floyd(int argc) {
    int dist[G_NMAX][G_NMAX];
    int pre[G_NMAX][G_NMAX];
    int negcycle = 0;
    if (!need_graph()) {
        return;
    }
    if (argc != 1) {
        fail("format");
        return;
    }
    say(floyd_run(&cur, dist, pre, &negcycle, stdout));
}

static void cmd_topo(int argc) {
    TopoInfo info;
    if (!need_graph()) {
        return;
    }
    if (argc != 1) {
        fail("format");
        return;
    }
    say(topo_run(&cur, &info, stdout));
}

static void cmd_topo_all(int argc) {
    int count = 0;
    if (!need_graph()) {
        return;
    }
    if (argc != 1) {
        fail("format");
        return;
    }
    say(topo_all(&cur, &count, stdout));
}

static void cmd_check(int argc, char **argv) {
    int seq[G_NMAX];
    if (!need_graph()) {
        return;
    }
    if (cur.kind != G_D) {
        fail("kind");
        return;
    }
    if (argc != cur.n + 1) {
        fail("format");
        return;
    }
    for (int i = 0; i < cur.n; i++) {
        if (!parse_int(argv[i + 1], &seq[i])) {
            fail("format");
            return;
        }
    }
    printf("%s\n", topo_check(&cur, seq, cur.n) ? "yes" : "no");
}

static void cmd_aoe(int argc) {
    Aoe net;
    if (!need_graph()) {
        return;
    }
    if (argc != 1) {
        fail("format");
        return;
    }
    say(aoe_run(&cur, &net, stdout));
}

void cli_dispatch(int argc, char **argv) {
    const char *cmd = argv[0];
    if (strcmp(cmd, "new") == 0) {
        cmd_new(argc, argv);
    } else if (strcmp(cmd, "edge") == 0) {
        cmd_edge(argc, argv);
    } else if (strcmp(cmd, "show") == 0) {
        cmd_show(argc);
    } else if (strcmp(cmd, "prim") == 0) {
        cmd_prim(argc, argv);
    } else if (strcmp(cmd, "kruskal") == 0) {
        cmd_kruskal(argc);
    } else if (strcmp(cmd, "dijkstra") == 0) {
        cmd_dijkstra(argc, argv);
    } else if (strcmp(cmd, "floyd") == 0) {
        cmd_floyd(argc);
    } else if (strcmp(cmd, "topo") == 0) {
        cmd_topo(argc);
    } else if (strcmp(cmd, "topo-all") == 0) {
        cmd_topo_all(argc);
    } else if (strcmp(cmd, "topo-check") == 0) {
        cmd_check(argc, argv);
    } else if (strcmp(cmd, "aoe") == 0) {
        cmd_aoe(argc);
    } else {
        fail("unknown");
    }
}
