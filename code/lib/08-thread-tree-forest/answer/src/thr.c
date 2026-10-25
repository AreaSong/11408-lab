#include "thr.h"

#include <ctype.h>
#include <string.h>

void thr_put_ptr(FILE *out, const ThrTree *t, int id);

void thr_init(ThrTree *t) {
    memset(t, 0, sizeof *t);
}

static int thr_alloc(ThrTree *t, char data, int parent) {
    if (t->n + 1 >= THR_HEAD) {
        return 0;
    }
    t->n++;
    t->node[t->n].data = data;
    t->node[t->n].parent = parent;
    return t->n;
}

static int take_pre(ThrTree *t, const char *s, int *i, int parent, int *out) {
    unsigned char ch = (unsigned char)s[*i];
    int id;
    int left = 0;
    int right = 0;
    int err;
    if (ch == '\0') {
        return THR_SHORT;
    }
    (*i)++;
    if (ch == '#') {
        *out = 0;
        return THR_OK;
    }
    if (!isgraph(ch)) {
        return THR_FORMAT;
    }
    for (int k = 1; k <= t->n; k++) {
        if (t->node[k].data == (char)ch) {
            return THR_DUP;
        }
    }
    id = thr_alloc(t, (char)ch, parent);
    if (id == 0) {
        return THR_SIZE;
    }
    err = take_pre(t, s, i, id, &left);
    if (err != THR_OK) {
        return err;
    }
    err = take_pre(t, s, i, id, &right);
    if (err != THR_OK) {
        return err;
    }
    t->node[id].left = left;
    t->node[id].right = right;
    *out = id;
    return THR_OK;
}

int thr_from_pre(ThrTree *t, const char *s) {
    int i = 0;
    int root = 0;
    int err;
    thr_init(t);
    if (s == NULL || s[0] == '\0') {
        return THR_EMPTY;
    }
    err = take_pre(t, s, &i, 0, &root);
    if (err != THR_OK) {
        thr_init(t);
        return err;
    }
    if (s[i] != '\0') {
        thr_init(t);
        return THR_EXTRA;
    }
    t->root = root;
    if (root != 0) {
        t->node[root].parent = 0;
    }
    return THR_OK;
}

int thr_find(const ThrTree *t, char data) {
    for (int i = 1; i <= t->n; i++) {
        if (t->node[i].data == data) {
            return i;
        }
    }
    return 0;
}

int thr_push(char *buf, int cap, int n, char ch) {
    if (n < 0 || n + 1 >= cap) {
        return -1;
    }
    buf[n] = ch;
    buf[n + 1] = '\0';
    return n + 1;
}

static int collect_in(const ThrTree *t, int id, char *buf, int cap, int n) {
    if (id == 0) {
        return n;
    }
    n = collect_in(t, t->node[id].left, buf, cap, n);
    if (n < 0) {
        return -1;
    }
    n = thr_push(buf, cap, n, t->node[id].data);
    if (n < 0) {
        return -1;
    }
    return collect_in(t, t->node[id].right, buf, cap, n);
}

int thr_inorder(const ThrTree *t, char *buf, int cap) {
    int n;
    if (cap <= 0) {
        return -1;
    }
    buf[0] = '\0';
    n = collect_in(t, t->root, buf, cap, 0);
    return n < 0 ? -1 : THR_OK;
}

void thr_collect_id(const ThrTree *t, int id, int *seq, int *n) {
    if (id == 0) {
        return;
    }
    thr_collect_id(t, t->node[id].left, seq, n);
    seq[(*n)++] = id;
    thr_collect_id(t, t->node[id].right, seq, n);
}

/* 中序先走左子树，此时 lchild 还是孩子。改完当前结点再走右子树，
 * 右孩子也还没被后继改成线索。pre 始终是刚访问过的结点。 */
static void in_visit(ThrTree *t, int id, int *pre, FILE *out, int *step) {
    int right;
    ThrNode *node;
    ThrNode *prev;
    if (id == 0) {
        return;
    }
    /* 先递归左子树。这时 lchild 仍是孩子，还没有被改成前驱线索。 */
    in_visit(t, t->node[id].left, pre, out, step);
    right = t->node[id].right;
    node = &t->node[id];
    if (node->left == 0) {
        node->left = *pre;
        node->ltag = 1;
    }
    prev = &t->node[*pre];
    if (out != NULL) {
        (*step)++;
        fprintf(out, "step %d visit=%c pre=", *step, node->data);
        thr_put_ptr(out, t, *pre);
        fprintf(out, " ltag=%d left=", node->ltag);
        thr_put_ptr(out, t, node->left);
    }
    /* 后继还没出现，所以改的是 pre 的右指针，不是当前结点自己的。 */
    if (prev->right == 0) {
        prev->right = id;
        prev->rtag = 1;
        if (out != NULL) {
            fprintf(out, " fix=");
            thr_put_ptr(out, t, *pre);
            fprintf(out, ">%c\n", node->data);
        }
    } else if (*pre != t->head) {
        prev->rtag = 0;
        if (out != NULL) {
            fprintf(out, " fix=");
            thr_put_ptr(out, t, *pre);
            fprintf(out, " child\n");
        }
    } else if (out != NULL) {
        fprintf(out, " fix=head\n");
    }
    *pre = id;
    in_visit(t, right, pre, out, step);
}

int thr_thread_in(ThrTree *t) {
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
    in_visit(t, t->root, &pre, NULL, &step);
    t->node[pre].right = t->head;
    t->node[pre].rtag = 1;
    t->node[t->head].right = pre;
    return THR_OK;
}

int thr_pred(const ThrTree *t, int id) {
    int p;
    if (t->node[id].ltag == 1) {
        return t->node[id].left;
    }
    /* 有左子树时，前驱是左子树最右下的结点。rtag 为 1 就不能再往右，那是线索。 */
    p = t->node[id].left;
    while (t->node[p].rtag == 0) {
        p = t->node[p].right;
    }
    return p;
}

int thr_succ(const ThrTree *t, int id) {
    int p;
    if (t->node[id].rtag == 1) {
        return t->node[id].right;
    }
    p = t->node[id].right;
    while (t->node[p].ltag == 0) {
        p = t->node[p].left;
    }
    return p;
}

void thr_put_ptr(FILE *out, const ThrTree *t, int id) {
    if (id == 0) {
        fputc('-', out);
    } else if (id == t->head) {
        fputc('^', out);
    } else {
        fputc(t->node[id].data, out);
    }
}

static int thread_and_trace(ThrTree *t, FILE *out) {
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
    in_visit(t, t->root, &pre, out, &step);
    t->node[pre].right = t->head;
    t->node[pre].rtag = 1;
    t->node[t->head].right = pre;
    fprintf(out, "close last=%c right=^\n", t->node[pre].data);
    return THR_OK;
}

static void walk_in(const ThrTree *t, char *buf, int cap) {
    int p = t->node[t->head].left;
    int n = 0;
    buf[0] = '\0';
    while (p != 0 && p != t->head) {
        while (t->node[p].ltag == 0) {
            p = t->node[p].left;
        }
        n = thr_push(buf, cap, n, t->node[p].data);
        while (t->node[p].rtag == 1 && t->node[p].right != t->head) {
            p = t->node[p].right;
            n = thr_push(buf, cap, n, t->node[p].data);
        }
        p = t->node[p].right;
    }
}

static void print_in_nodes(const ThrTree *t, const int *seq, int sn, FILE *out) {
    for (int i = 0; i < sn; i++) {
        int id = seq[i];
        fprintf(out, "node %c ltag=%d left=", t->node[id].data, t->node[id].ltag);
        thr_put_ptr(out, t, t->node[id].left);
        fprintf(out, " rtag=%d right=", t->node[id].rtag);
        thr_put_ptr(out, t, t->node[id].right);
        fprintf(out, " in_pred=%c in_succ=%c\n", i == 0 ? '-' : t->node[seq[i - 1]].data,
                i + 1 == sn ? '-' : t->node[seq[i + 1]].data);
    }
}

static void print_in_ask(const ThrTree *t, const int *seq, int sn, int ask_id, int ask, FILE *out) {
    int ip = -1;
    if (ask_id == 0) {
        return;
    }
    for (int i = 0; i < sn; i++) {
        if (seq[i] == ask_id) {
            ip = i;
        }
    }
    fprintf(out, "ask %c pred=%c succ=%c\n", (char)ask, ip <= 0 ? '-' : t->node[seq[ip - 1]].data,
            ip + 1 >= sn ? '-' : t->node[seq[ip + 1]].data);
}

int thr_show_in(FILE *out, const char *pre, int ask) {
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
    thr_collect_id(&tree, tree.root, seq, &sn);
    plain[0] = '\0';
    for (int i = 0; i < sn; i++) {
        thr_push(plain, THR_MAX, i, tree.node[seq[i]].data);
    }
    fprintf(out, "thread n=%d inorder=%s\n", tree.n, sn == 0 ? "-" : plain);
    if (tree.n == 0) {
        fprintf(out, "head ltag=0 left=^ rtag=1 right=^\n");
        return THR_OK;
    }
    thread_and_trace(&tree, out);
    print_in_nodes(&tree, seq, sn, out);
    fprintf(out, "head ltag=%d left=", tree.node[tree.head].ltag);
    thr_put_ptr(out, &tree, tree.node[tree.head].left);
    fprintf(out, " rtag=%d right=", tree.node[tree.head].rtag);
    thr_put_ptr(out, &tree, tree.node[tree.head].right);
    fputc('\n', out);
    walk_in(&tree, walked, THR_MAX);
    fprintf(out, "walk=%s\n", walked[0] == '\0' ? "-" : walked);
    fprintf(out, "check=%s\n", strcmp(walked, plain) == 0 ? "ok" : "bad");
    fprintf(out, "plain_null=2\n");
    print_in_ask(&tree, seq, sn, ask_id, ask, out);
    return THR_OK;
}
