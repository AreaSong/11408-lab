#include "walk.h"

#include "cycq.h"
#include "seq.h"
#include "sqstack.h"

static int add_ch(char *buf, int cap, int *n, int ch) {
    if (*n + 1 >= cap) {
        return -1;
    }
    buf[*n] = (char)ch;
    *n += 1;
    buf[*n] = '\0';
    return 0;
}

static void show_stack(const BiTree *t, const SqStack *S, FILE *out) {
    fputc('[', out);
    for (int i = 0; i <= S->top; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        int id = S->data[i];
        fprintf(out, "%d:%c", id, (char)t->node[id].data);
    }
    fputc(']', out);
}

static void show_queue(const BiTree *t, const CycQ *Q, FILE *out) {
    fputc('[', out);
    int i = Q->front;
    int first = 1;
    while (i != Q->rear) {
        if (!first) {
            fputc(' ', out);
        }
        first = 0;
        int id = Q->data[i];
        if (id == 0) {
            fprintf(out, "0:#");
        } else {
            fprintf(out, "%d:%c", id, (char)t->node[id].data);
        }
        i = (i + 1) % Q->cap;
    }
    fputc(']', out);
}

static void push_list(const BiTree *t, int right, int left, FILE *out) {
    if (right == 0 && left == 0) {
        fputc('-', out);
        return;
    }
    if (right != 0) {
        fputc((char)t->node[right].data, out);
    }
    if (left != 0) {
        if (right != 0) {
            fputc(',', out);
        }
        fputc((char)t->node[left].data, out);
    }
}

int walk_pre(const BiTree *t, char *buf, int cap, FILE *trace) {
    buf[0] = '\0';
    if (trace != NULL) {
        fprintf(trace, "pre_stack\n");
    }
    if (t->root == 0) {
        if (trace != NULL) {
            fprintf(trace, "pre\n");
        }
        return 0;
    }
    SqStack S;
    sq_init(&S, SQ_MAX);
    if (!sq_push(&S, t->root)) {
        return -1;
    }
    int n = 0;
    int step = 0;
    while (!sq_empty(&S)) {
        int id = 0;
        sq_pop(&S, &id);
        if (add_ch(buf, cap, &n, t->node[id].data) < 0) {
            return -1;
        }
        int right = t->node[id].right;
        int left = t->node[id].left;
        if (right != 0 && !sq_push(&S, right)) {
            return -1;
        }
        if (left != 0 && !sq_push(&S, left)) {
            return -1;
        }
        step++;
        if (trace != NULL) {
            fprintf(trace, "pre step=%d id=%d pop=%c visit=%c push=", step, id, (char)t->node[id].data,
                    (char)t->node[id].data);
            push_list(t, right, left, trace);
            fprintf(trace, " stack=");
            show_stack(t, &S, trace);
            fputc('\n', trace);
        }
    }
    if (trace != NULL) {
        fprintf(trace, "pre %s\n", buf);
    }
    return 0;
}

int walk_in(const BiTree *t, char *buf, int cap, FILE *trace) {
    buf[0] = '\0';
    if (trace != NULL) {
        fprintf(trace, "in_stack\n");
    }
    SqStack S;
    sq_init(&S, SQ_MAX);
    int id = t->root;
    int n = 0;
    int step = 0;
    while (id != 0 || !sq_empty(&S)) {
        if (id != 0) {
            if (!sq_push(&S, id)) {
                return -1;
            }
            step++;
            if (trace != NULL) {
                fprintf(trace, "in step=%d op=push id=%d ch=%c stack=", step, id, (char)t->node[id].data);
                show_stack(t, &S, trace);
                fputc('\n', trace);
            }
            id = t->node[id].left;
        } else {
            sq_pop(&S, &id);
            if (add_ch(buf, cap, &n, t->node[id].data) < 0) {
                return -1;
            }
            step++;
            if (trace != NULL) {
                fprintf(trace, "in step=%d op=visit id=%d ch=%c stack=", step, id, (char)t->node[id].data);
                show_stack(t, &S, trace);
                fputc('\n', trace);
            }
            id = t->node[id].right;
        }
    }
    if (trace != NULL) {
        fprintf(trace, "in %s\n", buf);
    }
    return 0;
}

static void post_line(const BiTree *t, const SqStack *S, int step, const char *op, int id, FILE *trace) {
    fprintf(trace, "post step=%d op=%s id=%d ch=%c stack=", step, op, id, (char)t->node[id].data);
    show_stack(t, S, trace);
    fputc('\n', trace);
}

int walk_post(const BiTree *t, char *buf, int cap, FILE *trace) {
    buf[0] = '\0';
    if (trace != NULL) {
        fprintf(trace, "post_stack\n");
    }
    SqStack S;
    sq_init(&S, SQ_MAX);
    int id = t->root;
    int last = 0;
    int n = 0;
    int step = 0;
    while (id != 0 || !sq_empty(&S)) {
        if (id != 0) {
            if (!sq_push(&S, id)) {
                return -1;
            }
            step++;
            if (trace != NULL) {
                post_line(t, &S, step, "push", id, trace);
            }
            id = t->node[id].left;
            continue;
        }
        int top = 0;
        sq_top(&S, &top);
        int right = t->node[top].right;
        if (right != 0 && right != last) {
            id = right;
            continue;
        }
        sq_pop(&S, &top);
        if (add_ch(buf, cap, &n, t->node[top].data) < 0) {
            return -1;
        }
        last = top;
        step++;
        if (trace != NULL) {
            post_line(t, &S, step, "visit", top, trace);
        }
    }
    if (trace != NULL) {
        fprintf(trace, "post %s\n", buf);
    }
    return 0;
}

static void level_line(const BiTree *t, const CycQ *Q, int layer, int id, FILE *trace) {
    fprintf(trace, "level layer=%d deq=%d:%c queue=", layer, id, (char)t->node[id].data);
    show_queue(t, Q, trace);
    fputc('\n', trace);
}

int walk_level(const BiTree *t, char *buf, int cap, int *width, FILE *trace) {
    buf[0] = '\0';
    *width = 0;
    if (trace != NULL) {
        fprintf(trace, "level_queue\n");
    }
    if (t->root == 0) {
        if (trace != NULL) {
            fprintf(trace, "level\nwidth=0\n");
        }
        return 0;
    }
    CycQ Q;
    cyc_init(&Q, CYC_MAX);
    if (!cyc_enq(&Q, t->root)) {
        return -1;
    }
    int n = 0;
    int layer = 0;
    while (!cyc_empty(&Q)) {
        int count = cyc_count(&Q);
        if (count > *width) {
            *width = count;
        }
        layer++;
        for (int k = 0; k < count; k++) {
            int id = 0;
            cyc_deq(&Q, &id);
            if (add_ch(buf, cap, &n, t->node[id].data) < 0) {
                return -1;
            }
            int left = t->node[id].left;
            int right = t->node[id].right;
            if (left != 0 && !cyc_enq(&Q, left)) {
                return -1;
            }
            if (right != 0 && !cyc_enq(&Q, right)) {
                return -1;
            }
            if (trace != NULL) {
                level_line(t, &Q, layer, id, trace);
            }
        }
    }
    if (trace != NULL) {
        fprintf(trace, "level %s\nwidth=%d\n", buf, *width);
    }
    return 0;
}

static void complete_line(const BiTree *t, const CycQ *Q, int id, int seen, FILE *trace) {
    if (id == 0) {
        fprintf(trace, "complete deq=null seen_null=%d queue=", seen);
    } else {
        fprintf(trace, "complete deq=%d:%c seen_null=%d queue=", id, (char)t->node[id].data, seen);
    }
    show_queue(t, Q, trace);
    fputc('\n', trace);
}

int walk_complete(const BiTree *t, FILE *trace) {
    if (trace != NULL) {
        fprintf(trace, "complete_scan\n");
    }
    if (t->root == 0) {
        if (trace != NULL) {
            fprintf(trace, "complete=1\n");
        }
        return 1;
    }
    CycQ Q;
    cyc_init(&Q, CYC_MAX);
    if (!cyc_enq(&Q, t->root)) {
        return -1;
    }
    int seen = 0;
    while (!cyc_empty(&Q)) {
        int id = 0;
        cyc_deq(&Q, &id);
        if (id == 0) {
            seen = 1;
            if (trace != NULL) {
                complete_line(t, &Q, 0, seen, trace);
            }
            continue;
        }
        if (seen) {
            if (trace != NULL) {
                fprintf(trace, "complete deq=%d:%c seen_null=1 reject\n", id, (char)t->node[id].data);
                fprintf(trace, "complete=0\n");
            }
            return 0;
        }
        if (!cyc_enq(&Q, t->node[id].left) || !cyc_enq(&Q, t->node[id].right)) {
            return -1;
        }
        if (trace != NULL) {
            complete_line(t, &Q, id, seen, trace);
        }
    }
    if (trace != NULL) {
        fprintf(trace, "complete=1\n");
    }
    return 1;
}

void walk_print(const BiTree *t, FILE *out) {
    char buf[BT_MAX];
    int width = 0;
    tree_trace(t, out);
    walk_pre(t, buf, BT_MAX, out);
    walk_in(t, buf, BT_MAX, out);
    walk_post(t, buf, BT_MAX, out);
    walk_level(t, buf, BT_MAX, &width, out);
    walk_complete(t, out);
}

void shape_print(const BiTree *t, FILE *out) {
    TreeInfo info;
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    char level[BT_MAX];
    int width = 0;
    tree_info(t, &info, pre, inord, post, BT_MAX);
    walk_level(t, level, BT_MAX, &width, NULL);
    int complete = walk_complete(t, NULL);
    fprintf(out, "shape n=%d n0=%d n1=%d n2=%d null=%d height=%d width=%d complete=%d\n", info.n, info.n0,
            info.n1, info.n2, info.nulls, info.height, width, complete);
    fprintf(out, "pre=%s in=%s post=%s\n", pre, inord, post);
    fprintf(out, "check n0=n2+1 ok=%d\n", info.n > 0 && info.n0 == info.n2 + 1);
    fprintf(out, "pre_eq_in=%d no_left=%d no_right=%d nonleaf=%d nonleaf_deg1=%d nonleaf_deg2=%d all_deg1=%d all_deg2=%d\n",
            info.pre_eq_in, info.no_left, info.no_right, info.nonleaf, info.nonleaf_deg1, info.nonleaf_deg2,
            info.all_deg1, info.all_deg2);
    SeqTree seq;
    if (seq_from_tree(t, &seq) != 0) {
        fprintf(out, "seq1 fail size\n");
        return;
    }
    seq_brief(&seq, out);
}
