#include "dag.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define DAG_MAX 48
#define DAG_TEXT 80

typedef struct {
    char op;
    char name;
    int left;
    int right;
    char text[DAG_TEXT];
} DagNode;

typedef struct {
    DagNode node[DAG_MAX];
    int n;
    const char *s;
    int i;
} Dag;

static int add_leaf(Dag *d, char name) {
    for (int k = 0; k < d->n; k++) {
        if (d->node[k].op == 0 && d->node[k].name == name) {
            return k;
        }
    }
    if (d->n >= DAG_MAX) {
        return -1;
    }
    d->node[d->n].op = 0;
    d->node[d->n].name = name;
    d->node[d->n].left = -1;
    d->node[d->n].right = -1;
    d->node[d->n].text[0] = name;
    d->node[d->n].text[1] = '\0';
    return d->n++;
}

static int add_op(Dag *d, char op, int left, int right) {
    int wrote;
    for (int k = 0; k < d->n; k++) {
        DagNode *node = &d->node[k];
        if (node->op == op && node->left == left && node->right == right) {
            return k;
        }
    }
    if (d->n >= DAG_MAX) {
        return -1;
    }
    wrote = snprintf(d->node[d->n].text, DAG_TEXT, "%c(%s,%s)", op, d->node[left].text,
                     d->node[right].text);
    if (wrote < 0 || wrote >= DAG_TEXT) {
        return -1;
    }
    d->node[d->n].op = op;
    d->node[d->n].name = 0;
    d->node[d->n].left = left;
    d->node[d->n].right = right;
    return d->n++;
}

static int parse_expr(Dag *d, int *out);

static int parse_factor(Dag *d, int *out) {
    char ch = d->s[d->i];
    if (ch == '(') {
        d->i++;
        if (!parse_expr(d, out) || d->s[d->i] != ')') {
            return 0;
        }
        d->i++;
        return 1;
    }
    if (!isalpha((unsigned char)ch)) {
        return 0;
    }
    *out = add_leaf(d, ch);
    d->i++;
    return *out >= 0;
}

static int parse_term(Dag *d, int *out) {
    int left;
    if (!parse_factor(d, &left)) {
        return 0;
    }
    while (d->s[d->i] == '*' || d->s[d->i] == '/') {
        char op = d->s[d->i++];
        int right;
        if (!parse_factor(d, &right)) {
            return 0;
        }
        left = add_op(d, op, left, right);
        if (left < 0) {
            return 0;
        }
    }
    *out = left;
    return 1;
}

static int parse_expr(Dag *d, int *out) {
    int left;
    if (!parse_term(d, &left)) {
        return 0;
    }
    while (d->s[d->i] == '+' || d->s[d->i] == '-') {
        char op = d->s[d->i++];
        int right;
        if (!parse_term(d, &right)) {
            return 0;
        }
        left = add_op(d, op, left, right);
        if (left < 0) {
            return 0;
        }
    }
    *out = left;
    return 1;
}

static int parse_dag(const char *expr, Dag *d, int *root) {
    memset(d, 0, sizeof *d);
    d->s = expr;
    if (expr == NULL || expr[0] == '\0' || !parse_expr(d, root) || d->s[d->i] != '\0') {
        return 0;
    }
    return 1;
}

int dag_vertices(const char *expr) {
    Dag dag;
    int root = 0;
    if (!parse_dag(expr, &dag, &root) || root < 0) {
        return -1;
    }
    return dag.n;
}

int dag_show(const char *expr, FILE *out) {
    Dag dag;
    int root = 0;
    if (!parse_dag(expr, &dag, &root) || root < 0) {
        return -1;
    }
    fprintf(out, "expr=%s\n", expr);
    for (int i = 0; i < dag.n; i++) {
        fprintf(out, "sub %s\n", dag.node[i].text);
    }
    fprintf(out, "vertices=%d\n", dag.n);
    return 0;
}
