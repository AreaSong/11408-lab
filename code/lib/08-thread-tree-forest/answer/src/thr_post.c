#include "thr.h"

#include <stdio.h>
#include <string.h>

void thr_put_ptr(FILE *out, const ThrTree *t, int id);
int thr_push(char *buf, int cap, int n, char ch);
void thr_collect_id(const ThrTree *t, int id, int *seq, int *n);

static void collect_post(const ThrTree *t, int id, int *seq, int *n) {
    if (id == 0) {
        return;
    }
    collect_post(t, t->node[id].left, seq, n);
    collect_post(t, t->node[id].right, seq, n);
    seq[(*n)++] = id;
}

static int go_side(const int *kid, int id) {
    int p = id;
    while (kid[p] != 0) {
        p = kid[p];
    }
    return p;
}

static void link_post(ThrTree *t, const int *sl, const int *sr, const int *seq, int sn) {
    for (int i = 0; i < sn; i++) {
        int id = seq[i];
        if (sl[id] == 0) {
            t->node[id].ltag = 1;
            t->node[id].left = i == 0 ? 0 : seq[i - 1];
        }
        if (sr[id] == 0) {
            t->node[id].rtag = 1;
            t->node[id].right = i + 1 == sn ? 0 : seq[i + 1];
        }
    }
}

static void print_post_nodes(const ThrTree *t, const int *seq, int sn, const int *sl, const int *sr, FILE *out) {
    for (int i = 0; i < sn; i++) {
        int id = seq[i];
        fprintf(out, "node %c ltag=%d left=", t->node[id].data, t->node[id].ltag);
        thr_put_ptr(out, t, t->node[id].ltag ? t->node[id].left : sl[id]);
        fprintf(out, " rtag=%d right=", t->node[id].rtag);
        thr_put_ptr(out, t, t->node[id].rtag ? t->node[id].right : sr[id]);
        fputc('\n', out);
    }
}

static void ask_post(const ThrTree *t, int ask_id, char ask, const int *sl, const int *sr, const int *seq,
                     const int *pos, FILE *out) {
    int parent = t->node[ask_id].parent;
    int y = 0;
    int left_most = 0;
    int right_most = 0;
    int right_to = t->node[ask_id].rtag ? t->node[ask_id].right : 0;
    char left_ch = '-';
    if (parent != 0 && sr[parent] == ask_id) {
        y = sl[parent];
    }
    if (y != 0) {
        left_most = go_side(sl, y);
        right_most = go_side(sr, y);
    }
    if (t->node[ask_id].ltag && t->node[ask_id].left != 0) {
        left_ch = t->node[seq[pos[ask_id] - 1]].data;
    }
    fprintf(out, "ask %c leaf=%d left_sibling=%c parent=%c\n", ask, sl[ask_id] == 0 && sr[ask_id] == 0,
            y == 0 ? '-' : t->node[y].data, parent == 0 ? '-' : t->node[parent].data);
    fprintf(out, "left_thread=%c right_thread=%c\n", left_ch, right_to == 0 ? '-' : t->node[right_to].data);
    fprintf(out, "y_leftmost=%c y_rightmost=%c\n", left_most == 0 ? '-' : t->node[left_most].data,
            right_most == 0 ? '-' : t->node[right_most].data);
}

int thr_show_post(FILE *out, const char *pre, int ask) {
    ThrTree tree;
    int seq[THR_MAX];
    int sn = 0;
    int sl[THR_MAX];
    int sr[THR_MAX];
    int pos[THR_MAX];
    int err = thr_from_pre(&tree, pre);
    int ask_id;
    char text[THR_MAX];
    if (err != THR_OK) {
        return err;
    }
    if (tree.n == 0) {
        return THR_EMPTY;
    }
    ask_id = thr_find(&tree, (char)ask);
    if (ask_id == 0) {
        return THR_MISS;
    }
    memset(sl, 0, sizeof sl);
    memset(sr, 0, sizeof sr);
    for (int i = 1; i <= tree.n; i++) {
        sl[i] = tree.node[i].left;
        sr[i] = tree.node[i].right;
    }
    collect_post(&tree, tree.root, seq, &sn);
    text[0] = '\0';
    for (int i = 0; i < sn; i++) {
        pos[seq[i]] = i;
        thr_push(text, THR_MAX, i, tree.node[seq[i]].data);
    }
    link_post(&tree, sl, sr, seq, sn);
    fprintf(out, "post n=%d order=%s\n", tree.n, text);
    print_post_nodes(&tree, seq, sn, sl, sr, out);
    ask_post(&tree, ask_id, (char)ask, sl, sr, seq, pos, out);
    return THR_OK;
}

static int direct_siblings(const ThrTree *t, int p, int q) {
    int parent = t->node[p].parent;
    if (parent == 0 || t->node[q].parent != parent) {
        return 0;
    }
    return t->node[parent].left == p && t->node[parent].right == q;
}

static void flag_pair(const ThrTree *t, int p, int q, FILE *out) {
    int parent = t->node[p].parent;
    int grand = parent != 0 && t->node[parent].parent == q;
    fprintf(out, "adj %c %c parent=%d right_child=%d sibling=%d grand=%d\n", t->node[p].data, t->node[q].data,
            t->node[p].parent == q, t->node[p].right == q, direct_siblings(t, p, q), grand);
}

int thr_show_pairs(FILE *out, const char *pre) {
    ThrTree tree;
    int seq[THR_MAX];
    int sn = 0;
    int err = thr_from_pre(&tree, pre);
    int sib = 0;
    if (err != THR_OK) {
        return err;
    }
    if (tree.n == 0) {
        return THR_EMPTY;
    }
    thr_collect_id(&tree, tree.root, seq, &sn);
    fprintf(out, "pair pre=%s inorder=", pre);
    for (int i = 0; i < sn; i++) {
        fputc(tree.node[seq[i]].data, out);
    }
    fputc('\n', out);
    for (int i = 0; i + 1 < sn; i++) {
        if (direct_siblings(&tree, seq[i], seq[i + 1])) {
            sib++;
        }
        flag_pair(&tree, seq[i], seq[i + 1], out);
    }
    fprintf(out, "sibling_adjacent=%d\n", sib);
    return THR_OK;
}

int thr_sibling_adj(const ThrTree *t) {
    int seq[THR_MAX];
    int sn = 0;
    int count = 0;
    thr_collect_id(t, t->root, seq, &sn);
    for (int i = 0; i + 1 < sn; i++) {
        if (direct_siblings(t, seq[i], seq[i + 1])) {
            count++;
        }
    }
    return count;
}

int thr_post_right(ThrTree *t, char ask, char *out_ch) {
    int seq[THR_MAX];
    int sn = 0;
    int sr[THR_MAX];
    int id = thr_find(t, ask);
    int at = -1;
    if (id == 0 || out_ch == NULL) {
        return THR_MISS;
    }
    for (int i = 1; i <= t->n; i++) {
        sr[i] = t->node[i].right;
    }
    collect_post(t, t->root, seq, &sn);
    for (int i = 0; i < sn; i++) {
        if (seq[i] == id) {
            at = i;
        }
        if (sr[seq[i]] == 0) {
            t->node[seq[i]].rtag = 1;
            t->node[seq[i]].right = i + 1 == sn ? 0 : seq[i + 1];
        }
    }
    if (at < 0 || t->node[id].rtag == 0 || t->node[id].right == 0) {
        *out_ch = 0;
        return THR_OK;
    }
    *out_ch = t->node[t->node[id].right].data;
    return THR_OK;
}

int thr_two_child_ok(const ThrTree *t) {
    int seq[THR_MAX];
    int sn = 0;
    int at[THR_MAX];
    thr_collect_id(t, t->root, seq, &sn);
    memset(at, 0, sizeof at);
    for (int i = 0; i < sn; i++) {
        at[seq[i]] = i;
    }
    for (int id = 1; id <= t->n; id++) {
        int ip;
        int pred;
        int succ;
        if (t->node[id].left == 0 || t->node[id].right == 0) {
            continue;
        }
        ip = at[id];
        if (ip <= 0 || ip + 1 >= sn) {
            return 0;
        }
        pred = seq[ip - 1];
        succ = seq[ip + 1];
        if (t->node[pred].right != 0 || t->node[succ].left != 0) {
            return 0;
        }
    }
    return 1;
}
