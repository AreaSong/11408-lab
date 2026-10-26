#include "aoe.h"
#include "graph.h"
#include "mst.h"
#include "path.h"
#include "topo.h"

#include "test.h"

#include <stdio.h>
#include <string.h>

static void arc(Graph *g, int u, int v, int w) {
    CHECK(graph_add(g, u, v, w) == G_OK);
}

static int has_edge(const Mst *m, int u, int v) {
    if (u > v) {
        int tmp = u;
        u = v;
        v = tmp;
    }
    for (int i = 0; i < m->nedge; i++) {
        if (m->u[i] == u && m->v[i] == v) {
            return 1;
        }
    }
    return 0;
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
    char key[160];
    snprintf(key, sizeof key, "%s\n", line);
    return strstr(buf, key) != NULL;
}

static void test_reject(void) {
    Graph g;
    graph_init(&g, G_U, 3);
    CHECK(graph_add(&g, 0, 0, 1) == G_LOOP);
    arc(&g, 0, 1, 4);
    CHECK(graph_add(&g, 1, 0, 4) == G_DUP);
    CHECK(graph_add(&g, 0, 1, 9) == G_DUP);
    CHECK(graph_add(&g, 0, 5, 1) == G_RANGE);
    CHECK(g.w[0][1] == 4);
    CHECK(g.w[1][0] == 4);
    CHECK(g.e == 1);
}

static void test_2021(void) {
    Graph g;
    Sp sp;
    graph_init(&g, G_D, 5);
    arc(&g, 0, 4, 6);
    arc(&g, 0, 1, 26);
    arc(&g, 0, 2, 3);
    arc(&g, 4, 1, 15);
    arc(&g, 4, 3, 8);
    arc(&g, 4, 2, 6);
    arc(&g, 3, 2, 6);
    arc(&g, 3, 1, 1);
    arc(&g, 2, 1, 22);
    CHECK(dijkstra_run(&g, 0, &sp, NULL) == G_OK);
    CHECK_EQ(sp.pick[0], 2);
    CHECK_EQ(sp.pick[1], 4);
    CHECK_EQ(sp.hist[2][1], 21);
    CHECK_EQ(sp.hist[2][2], 3);
    CHECK_EQ(sp.hist[2][3], 14);
    CHECK_EQ(sp.hist[2][4], 6);
    CHECK_EQ(sp.dist[1], 15);
    CHECK_EQ(sp.pre[1], 3);
}

static void test_2016(void) {
    Graph g;
    Sp sp;
    graph_init(&g, G_D, 6);
    arc(&g, 0, 4, 4);
    arc(&g, 0, 1, 5);
    arc(&g, 1, 2, 2);
    arc(&g, 4, 5, 5);
    arc(&g, 4, 3, 7);
    CHECK(dijkstra_run(&g, 0, &sp, NULL) == G_OK);
    CHECK_EQ(sp.npick, 5);
    CHECK_EQ(sp.pick[0], 4);
    CHECK_EQ(sp.pick[1], 1);
    CHECK_EQ(sp.pick[2], 2);
    CHECK_EQ(sp.pick[3], 5);
    CHECK_EQ(sp.pick[4], 3);
    CHECK_EQ(sp.hist[1][3], 11);
    CHECK_EQ(sp.hist[1][5], 9);
    CHECK_EQ(sp.dist[2], 7);
}

static void test_2015(void) {
    Graph g;
    Mst prim;
    Mst kru;
    FILE *fp = tmpfile();
    char buf[1024];
    graph_init(&g, G_U, 4);
    arc(&g, 2, 3, 8);
    arc(&g, 1, 2, 8);
    arc(&g, 0, 2, 8);
    arc(&g, 0, 3, 5);
    CHECK(fp != NULL);
    CHECK(prim_run(&g, 3, &prim, fp) == G_OK);
    CHECK(kruskal_run(&g, &kru, fp) == G_OK);
    CHECK(capture(fp, buf, (int)sizeof buf));
    fclose(fp);
    CHECK(has_line(buf, "round 2 pick 2 w=8 via 3 alt 0"));
    CHECK(has_line(buf, "next 8 open 0-2 1-2 2-3"));
    CHECK_EQ(prim.cost, 21);
    CHECK_EQ(kru.cost, 21);
    CHECK(prim.span && kru.span);
    CHECK(has_edge(&prim, 0, 3));
    CHECK(has_edge(&prim, 2, 3));
    CHECK(has_edge(&prim, 1, 2));
    CHECK(!has_edge(&prim, 0, 2));
    CHECK(has_edge(&kru, 0, 2));
    CHECK(!has_edge(&kru, 2, 3));
}

static void fill_2010(Graph *g, int with_ac) {
    graph_init(g, G_D, 5);
    arc(g, 0, 4, 1);
    arc(g, 0, 1, 1);
    if (with_ac) {
        arc(g, 0, 2, 1);
    }
    arc(g, 1, 2, 1);
    arc(g, 4, 3, 1);
    arc(g, 2, 3, 1);
}

static void test_2010(void) {
    Graph g;
    TopoInfo info;
    int count = 0;
    int seqs[3][5] = {{0, 4, 1, 2, 3}, {0, 1, 4, 2, 3}, {0, 1, 2, 4, 3}};
    int bad[5] = {0, 4, 2, 1, 3};
    fill_2010(&g, 1);
    CHECK(topo_run(&g, &info, NULL) == G_OK);
    CHECK_EQ(info.unique, 0);
    CHECK_EQ(info.cycle, 0);
    CHECK_EQ(info.order[0], 0);
    CHECK(topo_all(&g, &count, NULL) == G_OK);
    CHECK_EQ(count, 3);
    for (int i = 0; i < 3; i++) {
        CHECK(topo_check(&g, seqs[i], 5));
    }
    CHECK(!topo_check(&g, bad, 5));
    fill_2010(&g, 0);
    CHECK(topo_all(&g, &count, NULL) == G_OK);
    CHECK_EQ(count, 3);
}

static void test_neg(void) {
    Graph g;
    Sp sp;
    int dist[G_NMAX][G_NMAX];
    int pre[G_NMAX][G_NMAX];
    int negcycle = 0;
    graph_init(&g, G_D, 3);
    arc(&g, 0, 1, 4);
    arc(&g, 0, 2, 5);
    arc(&g, 2, 1, -3);
    CHECK(dijkstra_run(&g, 0, &sp, NULL) == G_OK);
    CHECK_EQ(sp.dist[1], 4);
    CHECK(floyd_run(&g, dist, pre, &negcycle, NULL) == G_OK);
    CHECK_EQ(negcycle, 0);
    CHECK_EQ(dist[0][1], 2);
    CHECK_EQ(pre[0][1], 2);
    graph_init(&g, G_D, 2);
    arc(&g, 0, 1, 1);
    arc(&g, 1, 0, -2);
    CHECK(floyd_run(&g, dist, pre, &negcycle, NULL) == G_OK);
    CHECK_EQ(negcycle, 1);
    CHECK(dist[0][0] < 0);
}

static void aoe_net(Graph *g, int b, int d) {
    graph_init(g, G_D, 6);
    arc(g, 0, 1, b);
    arc(g, 0, 3, 1);
    arc(g, 1, 2, d);
    arc(g, 1, 4, 4);
    arc(g, 2, 3, 2);
    arc(g, 2, 4, 2);
    arc(g, 3, 5, 3);
    arc(g, 4, 5, 3);
}

static int act_slack(const Aoe *net, int u, int v) {
    for (int i = 0; i < net->nact; i++) {
        if (net->act[i].u == u && net->act[i].v == v) {
            return net->act[i].slack;
        }
    }
    return -1;
}

static void test_aoe(void) {
    Graph g;
    Aoe net;
    int ve[6] = {0, 3, 5, 7, 7, 10};
    aoe_net(&g, 3, 2);
    CHECK(aoe_run(&g, &net, NULL) == G_OK);
    CHECK_EQ(net.cycle, 0);
    CHECK_EQ(net.len, 10);
    CHECK_EQ(net.npath, 3);
    CHECK_EQ(net.nalone, 1);
    CHECK_EQ(net.alone_u[0], 0);
    CHECK_EQ(net.alone_v[0], 1);
    CHECK_EQ(act_slack(&net, 0, 3), 6);
    for (int i = 0; i < 6; i++) {
        CHECK_EQ(net.ve[i], ve[i]);
        CHECK_EQ(net.vl[i], ve[i]);
    }
    aoe_net(&g, 3, 3);
    CHECK(aoe_run(&g, &net, NULL) == G_OK);
    CHECK_EQ(net.len, 11);
    aoe_net(&g, 3, 1);
    CHECK(aoe_run(&g, &net, NULL) == G_OK);
    CHECK_EQ(net.len, 10);
    aoe_net(&g, 2, 2);
    CHECK(aoe_run(&g, &net, NULL) == G_OK);
    CHECK_EQ(net.len, 9);
}

static void test_tie_and_unit(void) {
    Graph g;
    Mst a;
    Mst b;
    Sp sp;
    graph_init(&g, G_U, 3);
    arc(&g, 0, 1, 1);
    arc(&g, 1, 2, 1);
    arc(&g, 2, 0, 1);
    CHECK(prim_run(&g, 0, &a, NULL) == G_OK);
    CHECK(prim_run(&g, 1, &b, NULL) == G_OK);
    CHECK_EQ(a.cost, 2);
    CHECK_EQ(b.cost, 2);
    CHECK(has_edge(&a, 0, 1) && has_edge(&a, 0, 2) && !has_edge(&a, 1, 2));
    CHECK(has_edge(&b, 0, 1) && has_edge(&b, 1, 2) && !has_edge(&b, 0, 2));
    graph_init(&g, G_U, 4);
    arc(&g, 0, 1, 1);
    arc(&g, 1, 2, 1);
    arc(&g, 2, 3, 1);
    arc(&g, 3, 0, 1);
    CHECK(kruskal_run(&g, &a, NULL) == G_OK);
    CHECK(!has_edge(&a, 2, 3));
    CHECK(dijkstra_run(&g, 2, &sp, NULL) == G_OK);
    CHECK_EQ(sp.dist[3], 1);
    graph_init(&g, G_U, 3);
    arc(&g, 0, 2, 4);
    arc(&g, 1, 2, 3);
    arc(&g, 0, 1, 1);
    CHECK(prim_run(&g, 0, &a, NULL) == G_OK);
    CHECK(kruskal_run(&g, &b, NULL) == G_OK);
    CHECK_EQ(a.cost, b.cost);
    CHECK(has_edge(&a, 0, 1) && has_edge(&a, 1, 2) && !has_edge(&a, 0, 2));
    CHECK(has_edge(&b, 0, 1) && has_edge(&b, 1, 2) && !has_edge(&b, 0, 2));
}

static void test_floyd_step(void) {
    Graph g;
    int dist[G_NMAX][G_NMAX];
    int pre[G_NMAX][G_NMAX];
    int negcycle = 1;
    graph_init(&g, G_D, 3);
    arc(&g, 0, 1, 3);
    arc(&g, 0, 2, 8);
    arc(&g, 1, 2, 2);
    CHECK(floyd_run(&g, dist, pre, &negcycle, NULL) == G_OK);
    CHECK_EQ(negcycle, 0);
    CHECK_EQ(dist[0][2], 5);
    CHECK_EQ(pre[0][2], 1);
}

static void test_cycle(void) {
    Graph g;
    TopoInfo info;
    int count = 9;
    graph_init(&g, G_D, 2);
    arc(&g, 0, 1, 1);
    arc(&g, 1, 0, 1);
    CHECK(topo_run(&g, &info, NULL) == G_OK);
    CHECK_EQ(info.cycle, 1);
    CHECK_EQ(info.norder, 0);
    CHECK(topo_all(&g, &count, NULL) == G_OK);
    CHECK_EQ(count, 0);
    graph_init(&g, G_D, 3);
    arc(&g, 0, 1, 1);
    arc(&g, 1, 2, 1);
    CHECK(topo_run(&g, &info, NULL) == G_OK);
    CHECK_EQ(info.unique, 1);
    CHECK_EQ(info.cycle, 0);
}

int main(void) {
    test_reject();
    test_2021();
    test_2016();
    test_2015();
    test_2010();
    test_neg();
    test_aoe();
    test_tie_and_unit();
    test_floyd_step();
    test_cycle();
    return TEST_SUMMARY();
}
