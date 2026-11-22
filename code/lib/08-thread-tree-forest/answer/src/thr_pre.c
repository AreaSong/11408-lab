#include "thr.h"

#include <string.h>

void thr_put_ptr(FILE *out, const ThrTree *t, int id);
int thr_push(char *buf, int cap, int n, char ch);

static void collect_pre(const ThrTree *t, int id, int *seq, int *n) {
    if (id == 0) {
        return;
    }
    seq[(*n)++] = id;
    collect_pre(t, t->node[id].left, seq, n);
    collect_pre(t, t->node[id].right, seq, n);
}

static void trace_step(FILE *out, ThrTree *t, int id, int pre, int *step) {
    if (out == NULL) {
        return;
    }
    (*step)++;
    fprintf(out, "step %d visit=%c pre=", *step, t->node[id].data);
    thr_put_ptr(out, t, pre);
    fprintf(out, " ltag=%d left=", t->node[id].ltag);
    thr_put_ptr(out, t, t->node[id].left);
}

static void trace_fix(FILE *out, ThrTree *t, int pre, int id, int threaded) {
    if (out == NULL) {
        return;
    }
    if (pre == t->head && !threaded) {
        fprintf(out, " fix=head\n");
        return;
    }
    fprintf(out, " fix=");
    thr_put_ptr(out, t, pre);
    if (threaded) {
        fprintf(out, ">%c\n", t->node[id].data);
    } else {
        fprintf(out, " child\n");
    }
}

/* 先序是先访问再下钻。左、右指针要先抄下来，因为空孩子马上会被改成线索。 */
static void pre_visit(ThrTree *t, int id, int *pre, FILE *out, int *step) {
    int left;
    int right;
    ThrNode *node;
    ThrNode *prev;
    int threaded = 0;
    if (id == 0) {
        return;
    }
    left = t->node[id].left;
    right = t->node[id].right;
    node = &t->node[id];
    if (node->left == 0) {
        node->left = *pre;
        node->ltag = 1;
    }
    prev = &t->node[*pre];
    trace_step(out, t, id, *pre, step);
    if (prev->right == 0) {
        prev->right = id;
        prev->rtag = 1;
        threaded = 1;
    } else if (*pre != t->head) {
        prev->rtag = 0;
    }
    trace_fix(out, t, *pre, id, threaded);
    *pre = id;
    if (node->ltag == 0) {
        pre_visit(t, left, pre, out, step);
    }
    /* 左子树里的后继可能已经把空的右孩子改成线索，这时不能再顺着走。 */
    if (node->rtag == 0) {
        pre_visit(t, right, pre, out, step);
    }
}

static int thread_pre_core(ThrTree *t, FILE *out) {
    int pre;
    int step = 0;
    t->head = THR_HEAD;
    t->node[t->head].data = 0;
    t->node[t->head].ltag = 0;
    t->node[t->head].rtag = 1;
    t->node[t->head].right = t->head;
    if (t->root == 0) {
        t->node[t->head].left = t->head;
        return THR_OK;
    }
    t->node[t->head].left = t->root;
    pre = t->head;
    pre_visit(t, t->root, &pre, out, &step);
    t->node[pre].right = t->head;
    t->node[pre].rtag = 1;
    t->node[t->head].right = pre;
    if (out != NULL) {
        fprintf(out, "close last=%c right=^\n", t->node[pre].data);
    }
    return THR_OK;
}

int thr_thread_pre(ThrTree *t) {
    return thread_pre_core(t, NULL);
}

int thr_pre_succ(const ThrTree *t, int id) {
    if (t->node[id].ltag == 0) {
        return t->node[id].left;
    }
    return t->node[id].right;
}

static int last_pre(const ThrTree *t, int id) {
    while (1) {
        if (t->node[id].rtag == 0 && t->node[id].right != 0) {
            id = t->node[id].right;
        } else if (t->node[id].ltag == 0 && t->node[id].left != 0) {
            id = t->node[id].left;
        } else {
            return id;
        }
    }
}

int thr_pre_pred(const ThrTree *t, int id) {
    int parent;
    if (t->node[id].ltag == 1) {
        return t->node[id].left;
    }
    /* 有左孩子时左指针是孩子。先序前驱不在这条线索里，只能从双亲往回看。 */
    parent = t->node[id].parent;
    if (parent == 0) {
        return t->head;
    }
    if (t->node[parent].ltag == 0 && t->node[parent].left == id) {
        return parent;
    }
    if (t->node[parent].ltag == 1) {
        return parent;
    }
    return last_pre(t, t->node[parent].left);
}

static void walk_pre(const ThrTree *t, char *buf, int cap) {
    int p = t->node[t->head].left;
    int n = 0;
    buf[0] = '\0';
    while (p != 0 && p != t->head) {
        n = thr_push(buf, cap, n, t->node[p].data);
        if (t->node[p].ltag == 0) {
            p = t->node[p].left;
        } else {
            p = t->node[p].right;
        }
    }
}

static char show_ch(const ThrTree *t, int id) {
    if (id == 0 || id == t->head) {
        return '-';
    }
    return t->node[id].data;
}

static void print_pre_nodes(const ThrTree *t, const int *seq, int sn, FILE *out) {
    for (int i = 0; i < sn; i++) {
        int id = seq[i];
        const char *via = t->node[id].ltag == 1 ? "thread" : "parent";
        fprintf(out, "node %c ltag=%d left=", t->node[id].data, t->node[id].ltag);
        thr_put_ptr(out, t, t->node[id].left);
        fprintf(out, " rtag=%d right=", t->node[id].rtag);
        thr_put_ptr(out, t, t->node[id].right);
        fprintf(out, " pre_pred=%c pre_succ=%c pred_via=%s\n", i == 0 ? '-' : t->node[seq[i - 1]].data,
                i + 1 == sn ? '-' : t->node[seq[i + 1]].data, via);
    }
}

static void print_pre_ask(const ThrTree *t, int ask_id, int ask, FILE *out) {
    int pred;
    int succ;
    if (ask_id == 0) {
        return;
    }
    pred = thr_pre_pred(t, ask_id);
    succ = thr_pre_succ(t, ask_id);
    fprintf(out, "ask %c pred=%c succ=%c pred_via=%s\n", (char)ask, show_ch(t, pred), show_ch(t, succ),
            t->node[ask_id].ltag == 1 ? "thread" : "parent");
}

int thr_show_pre(FILE *out, const char *pre, int ask) {
    ThrTree tree;
    int seq[THR_MAX];
    int sn = 0;
    char plain[THR_MAX];
    char walked[THR_MAX];
    int err = thr_from_pre(&tree, pre);
    int ask_id = 0;
    if (err != THR_OK) {
        return err;
    }
    if (ask != 0) {
        ask_id = thr_find(&tree, (char)ask);
        if (ask_id == 0) {
            return THR_MISS;
        }
    }
    collect_pre(&tree, tree.root, seq, &sn);
    plain[0] = '\0';
    for (int i = 0; i < sn; i++) {
        thr_push(plain, THR_MAX, i, tree.node[seq[i]].data);
    }
    fprintf(out, "prethread n=%d preorder=%s\n", tree.n, sn == 0 ? "-" : plain);
    if (tree.n == 0) {
        fprintf(out, "head ltag=0 left=^ rtag=1 right=^\n");
        fprintf(out, "pred_rule=ltag1-uses-left-thread; ltag0-needs-parent\n");
        return THR_OK;
    }
    thread_pre_core(&tree, out);
    print_pre_nodes(&tree, seq, sn, out);
    fprintf(out, "head ltag=%d left=", tree.node[tree.head].ltag);
    thr_put_ptr(out, &tree, tree.node[tree.head].left);
    fprintf(out, " rtag=%d right=", tree.node[tree.head].rtag);
    thr_put_ptr(out, &tree, tree.node[tree.head].right);
    fputc('\n', out);
    walk_pre(&tree, walked, THR_MAX);
    fprintf(out, "walk=%s\n", walked[0] == '\0' ? "-" : walked);
    fprintf(out, "check=%s\n", strcmp(walked, plain) == 0 ? "ok" : "bad");
    fprintf(out, "pred_rule=ltag1-uses-left-thread; ltag0-needs-parent\n");
    print_pre_ask(&tree, ask_id, ask, out);
    return THR_OK;
}
