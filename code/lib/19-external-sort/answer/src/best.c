#include "best.h"

#include "balance.h"

#include <string.h>

#define BEST_NODES 64

typedef struct {
    long long weight;
    int used;
    int seq;
} Node;

typedef struct {
    long long weight;
    int seq;
} Part;

static int add_node(Node *node, int *n, long long weight) {
    if (*n >= BEST_NODES) {
        return -1;
    }
    node[*n].weight = weight;
    node[*n].used = 0;
    node[*n].seq = *n;
    (*n)++;
    return 0;
}

static int alive_count(const Node *node, int n) {
    int i;
    int count = 0;
    for (i = 0; i < n; i++) {
        if (!node[i].used) {
            count++;
        }
    }
    return count;
}

/* 权值小的先拿。权值相同则拿更早生成的，虚段序号最小，会最先被配走。 */
static int pick_one(const Node *node, int n) {
    int i;
    int best = -1;
    for (i = 0; i < n; i++) {
        if (node[i].used) {
            continue;
        }
        if (best < 0 || node[i].weight < node[best].weight ||
            (node[i].weight == node[best].weight && node[i].seq < node[best].seq)) {
            best = i;
        }
    }
    return best;
}

static void order_parts(Part *part, int n) {
    int i;
    for (i = 1; i < n; i++) {
        Part item = part[i];
        int j = i;
        while (j > 0 && (part[j - 1].weight > item.weight ||
                         (part[j - 1].weight == item.weight && part[j - 1].seq > item.seq))) {
            part[j] = part[j - 1];
            j--;
        }
        part[j] = item;
    }
}

static int merge_once(Node *node, int *total, BestTree *out) {
    Part part[BEST_WAYS];
    long long sum = 0;
    int j;
    if (alive_count(node, *total) < out->k || out->steps >= BEST_STEPS) {
        return -1;
    }
    for (j = 0; j < out->k; j++) {
        int id = pick_one(node, *total);
        if (id < 0) {
            return -1;
        }
        node[id].used = 1;
        part[j].weight = node[id].weight;
        part[j].seq = node[id].seq;
        sum += part[j].weight;
    }
    order_parts(part, out->k);
    for (j = 0; j < out->k; j++) {
        out->part[out->steps][j] = part[j].weight;
    }
    out->made[out->steps] = sum;
    out->wpl += sum;
    out->steps++;
    return add_node(node, total, sum);
}

int best_tree(int ways, const long long *len, int runs, BestTree *out) {
    Node node[BEST_NODES];
    int total = 0;
    int dummy;
    int i;
    if (ways < 2 || ways > BEST_WAYS || runs < 1 || runs > BEST_RUNS) {
        return -1;
    }
    dummy = dummy_count(runs, ways);
    if (dummy < 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->k = ways;
    out->runs = runs;
    out->dummy = dummy;
    for (i = 0; i < dummy; i++) {
        if (add_node(node, &total, 0) != 0) {
            return -1;
        }
    }
    for (i = 0; i < runs; i++) {
        if (len[i] <= 0 || len[i] > BEST_LEN_MAX || add_node(node, &total, len[i]) != 0) {
            return -1;
        }
    }
    while (alive_count(node, total) > 1) {
        if (merge_once(node, &total, out) != 0) {
            return -1;
        }
    }
    out->io = out->wpl * 2;
    /* 二路合并最坏要比到两个表都剩最后一个，比较次数是长度和减 1，比内部结点权值少 1。 */
    out->cmp_worst = (ways == 2) ? out->wpl - out->steps : -1;
    return 0;
}

void best_print(const BestTree *tree, FILE *out) {
    int step;
    int i;
    fprintf(out, "dummy=%d\n", tree->dummy);
    for (step = 0; step < tree->steps; step++) {
        fprintf(out, "merge");
        for (i = 0; i < tree->k; i++) {
            fprintf(out, " %lld", tree->part[step][i]);
        }
        fprintf(out, " -> %lld\n", tree->made[step]);
    }
    fprintf(out, "wpl=%lld\n", tree->wpl);
    fprintf(out, "io=%lld\n", tree->io);
    if (tree->k == 2) {
        fprintf(out, "cmp_worst=%lld\n", tree->cmp_worst);
    }
}
