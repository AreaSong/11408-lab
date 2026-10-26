#include "cross.h"
#include "enumdfs.h"
#include "fact.h"
#include "graph.h"
#include "multi.h"
#include "walk.h"

#include "test.h"

#include <stdio.h>
#include <string.h>

static void zero(Graph *g) {
    memset(g, 0, sizeof *g);
}

static int capture(FILE *fp, char *buf, int cap) {
    long n;
    fflush(fp);
    n = ftell(fp);
    if (n < 0 || n >= cap) {
        return 0;
    }
    rewind(fp);
    if (n > 0 && fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        return 0;
    }
    buf[n] = '\0';
    return 1;
}

static int has_line(const char *buf, const char *line) {
    char key[128];
    snprintf(key, sizeof key, "%s\n", line);
    return strstr(buf, key) != NULL;
}

static int chain_matches(const Graph *g) {
    for (int u = 0; u < g->n; u++) {
        const ArcNode *p = g->head[u];
        for (int i = 0; i < g->len[u]; i++) {
            if (p == NULL || p->adjvex != g->adj[u][i]) {
                return 0;
            }
            p = p->nextarc;
        }
        if (p != NULL) {
            return 0;
        }
    }
    return 1;
}

static void add_ok(Graph *g, int u, int v) {
    CHECK(graph_add(g, u, v) == ADD_OK);
}

static void test_list(void) {
    Graph g;
    int sum = 0;
    zero(&g);
    graph_init(&g, G_U, 4);
    add_ok(&g, 0, 2);
    add_ok(&g, 0, 1);
    add_ok(&g, 1, 3);
    add_ok(&g, 2, 3);
    CHECK(g.e == 4);
    CHECK(g.adj[0][0] == 2);
    CHECK(g.adj[0][1] == 1);
    CHECK(chain_matches(&g));
    for (int u = 0; u < g.n; u++) {
        sum += g.len[u];
        CHECK(g.mat[u][u] == 0);
        for (int v = 0; v < g.n; v++) {
            CHECK(g.mat[u][v] == g.mat[v][u]);
        }
    }
    CHECK(sum == 8);
    CHECK(graph_add(&g, 0, 0) == ADD_LOOP);
    CHECK(graph_add(&g, 0, 1) == ADD_DUP);
    CHECK(graph_add(&g, 0, 9) == ADD_RANGE);
    graph_clear(&g);
}

static void test_order(void) {
    Graph g;
    char dfs_buf[4096];
    char stack_buf[4096];
    char flip_buf[4096];
    char bfs_buf[4096];
    FILE *fp;
    zero(&g);
    graph_init(&g, G_U, 4);
    add_ok(&g, 0, 2);
    add_ok(&g, 0, 1);
    add_ok(&g, 1, 3);
    add_ok(&g, 2, 3);
    fp = tmpfile();
    walk_dfs(&g, 0, fp);
    CHECK(capture(fp, dfs_buf, (int)sizeof dfs_buf));
    fclose(fp);
    fp = tmpfile();
    walk_dfs_stack(&g, 0, fp);
    CHECK(capture(fp, stack_buf, (int)sizeof stack_buf));
    fclose(fp);
    fp = tmpfile();
    walk_dfs_flip(&g, 0, fp);
    CHECK(capture(fp, flip_buf, (int)sizeof flip_buf));
    fclose(fp);
    fp = tmpfile();
    walk_bfs(&g, 0, fp);
    CHECK(capture(fp, bfs_buf, (int)sizeof bfs_buf));
    fclose(fp);
    CHECK(has_line(dfs_buf, "order 0 2 3 1"));
    CHECK(has_line(dfs_buf, "tree 0-2 2-3 3-1"));
    CHECK(has_line(stack_buf, "order 0 2 3 1"));
    CHECK(has_line(stack_buf, "tree 0-2 2-3 3-1"));
    CHECK(has_line(flip_buf, "order 0 1 3 2"));
    CHECK(has_line(bfs_buf, "order 0 2 1 3"));
    CHECK(has_line(bfs_buf, "tree 0-2 0-1 2-3"));
    graph_canon(&g);
    CHECK(g.adj[0][0] == 1);
    CHECK(g.adj[0][1] == 2);
    CHECK(g.mat[0][2] == 1);
    fp = tmpfile();
    walk_dfs(&g, 0, fp);
    CHECK(capture(fp, dfs_buf, (int)sizeof dfs_buf));
    fclose(fp);
    CHECK(has_line(dfs_buf, "order 0 1 3 2"));
    graph_clear(&g);
}

static void test_2015(void) {
    Graph g;
    char buf[1024];
    FILE *fp = tmpfile();
    zero(&g);
    graph_init(&g, G_D, 4);
    add_ok(&g, 0, 1);
    add_ok(&g, 0, 2);
    add_ok(&g, 0, 3);
    add_ok(&g, 1, 3);
    CHECK(enum_dfs(&g, 0, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "perms=6"));
    CHECK(has_line(buf, "count=5"));
    CHECK(has_line(buf, "0 1 3 2"));
    CHECK(has_line(buf, "0 2 1 3"));
    CHECK(has_line(buf, "0 2 3 1"));
    CHECK(has_line(buf, "0 3 1 2"));
    CHECK(has_line(buf, "0 3 2 1"));
    CHECK(strstr(buf, "0 1 2 3") == NULL);
    CHECK(g.adj[0][0] == 1);
    fp = tmpfile();
    walk_dfs(&g, 0, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "order 0 1 3 2"));
    CHECK(has_line(buf, "tree 0>1 1>3 0>2"));
    graph_clear(&g);
}

static void test_cross_multi(void) {
    Graph g;
    char buf[2048];
    FILE *fp;
    zero(&g);
    graph_init(&g, G_D, 3);
    add_ok(&g, 0, 1);
    add_ok(&g, 0, 2);
    add_ok(&g, 2, 1);
    CHECK(multi_print(&g, stdout) == -1);
    fp = tmpfile();
    CHECK(cross_print(&g, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "arc-nodes=3"));
    CHECK(has_line(buf, "out 2 0 1"));
    CHECK(has_line(buf, "in 0 2 1"));
    CHECK(has_line(buf, "box 0>1 tnext=0>2 hnext=2>1"));
    graph_clear(&g);

    zero(&g);
    graph_init(&g, G_U, 4);
    add_ok(&g, 0, 1);
    add_ok(&g, 1, 2);
    add_ok(&g, 0, 2);
    add_ok(&g, 2, 3);
    CHECK(cross_print(&g, stdout) == -1);
    fp = tmpfile();
    CHECK(multi_print(&g, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "edge-nodes=4"));
    CHECK(has_line(buf, "list-nodes=8"));
    CHECK(has_line(buf, "v0 deg=2 chain=0-1,0-2"));
    CHECK(has_line(buf, "v1 deg=2 chain=0-1,1-2"));
    CHECK(has_line(buf, "v2 deg=3 chain=1-2,0-2,2-3"));
    CHECK(has_line(buf, "v3 deg=1 chain=2-3"));
    CHECK(has_line(buf, "hits 0=2 1=2 2=3 3=1"));
    CHECK(has_line(buf, "seen-from-ends=8"));
    CHECK(has_line(buf, "marked-once=4"));
    graph_clear(&g);
}

static void test_strong(void) {
    Graph g;
    char buf[1024];
    FILE *fp;
    zero(&g);
    graph_init(&g, G_D, 3);
    add_ok(&g, 0, 1);
    add_ok(&g, 1, 2);
    add_ok(&g, 2, 0);
    fp = tmpfile();
    walk_reach(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "strong=yes"));
    CHECK(has_line(buf, "dfs-starts=1"));
    graph_clear(&g);
    zero(&g);
    graph_init(&g, G_D, 3);
    add_ok(&g, 0, 1);
    add_ok(&g, 1, 2);
    fp = tmpfile();
    walk_reach(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "dfs-starts=1"));
    CHECK(has_line(buf, "from-2=1"));
    CHECK(has_line(buf, "strong=no"));
    CHECK(has_line(buf, "weak=yes"));
    graph_clear(&g);
}

static void test_forest(void) {
    Graph g;
    char buf[1024];
    FILE *fp;
    zero(&g);
    graph_init(&g, G_U, 5);
    add_ok(&g, 0, 1);
    add_ok(&g, 2, 3);
    fp = tmpfile();
    walk_bfs_all(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "starts=3"));
    CHECK(has_line(buf, "visits=5"));
    CHECK(has_line(buf, "scans=4"));
    CHECK(has_line(buf, "list-nodes=4"));
    fp = tmpfile();
    walk_reach(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "components=3"));
    CHECK(has_line(buf, "connected=no"));
    graph_clear(&g);
}

static void test_facts(void) {
    char buf[1024];
    FILE *fp = tmpfile();
    int deg_bad[] = {3, 1};
    int cnt_bad[] = {2, 2};
    int deg_2017[] = {4, 3};
    int cnt_2017[] = {3, 4};
    LeastIn bad = {4, 2, deg_bad, cnt_bad, 2};
    LeastIn exam = {16, 2, deg_2017, cnt_2017, 2};
    CHECK(fact_bound(7, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "tree=6"));
    CHECK(has_line(buf, "disc-max=15"));
    CHECK(has_line(buf, "guarantee=16"));
    CHECK(has_line(buf, "complete-u=21"));
    fp = tmpfile();
    CHECK(fact_enum(3, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "graphs=8"));
    CHECK(has_line(buf, "connected=4"));
    CHECK(has_line(buf, "i-always=yes"));
    CHECK(has_line(buf, "ii-always=no"));
    CHECK(has_line(buf, "iii-always=no"));
    CHECK(has_line(buf, "conn-e-gt=1"));
    CHECK(has_line(buf, "conn-deg1=3"));
    fp = tmpfile();
    CHECK(fact_bound_check(4, fp) == 0);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "seen-disc-max=3"));
    CHECK(has_line(buf, "guarantee-holds=yes"));
    fp = tmpfile();
    fact_least(&exam, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "vertices=11"));
    CHECK(has_line(buf, "havel=yes"));
    CHECK(has_line(buf, "deg-sum=32"));
    CHECK(has_line(buf, "seq 4 4 4 3 3 3 3 2 2 2 2"));
    fp = tmpfile();
    fact_least(&bad, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "vertices=4"));
    CHECK(has_line(buf, "havel=no"));
}

static void test_enum_cap(void) {
    Graph g;
    FILE *fp = tmpfile();
    zero(&g);
    graph_init(&g, G_U, 9);
    for (int v = 1; v <= 8; v++) {
        add_ok(&g, 0, v);
    }
    CHECK(enum_dfs(&g, 0, fp) == -1);
    CHECK(ftell(fp) == 0);
    fclose(fp);
    graph_clear(&g);
}

static void test_directed_deg(void) {
    Graph g;
    char buf[512];
    FILE *fp = tmpfile();
    zero(&g);
    graph_init(&g, G_D, 4);
    add_ok(&g, 0, 1);
    add_ok(&g, 0, 2);
    add_ok(&g, 1, 2);
    add_ok(&g, 1, 3);
    add_ok(&g, 2, 3);
    graph_deg(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "sum-out=5"));
    CHECK(has_line(buf, "sum-in=5"));
    CHECK(has_line(buf, "handshake=yes"));
    fp = tmpfile();
    walk_bfs_all(&g, fp);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "visits=4"));
    CHECK(has_line(buf, "scans=5"));
    CHECK(has_line(buf, "list-nodes=5"));
    CHECK(has_line(buf, "order 0 1 2 3"));
    graph_clear(&g);
}

int main(void) {
    test_list();
    test_order();
    test_2015();
    test_cross_multi();
    test_strong();
    test_forest();
    test_facts();
    test_enum_cap();
    test_directed_deg();
    return TEST_SUMMARY();
}
