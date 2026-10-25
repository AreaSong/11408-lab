#include "seq.h"

#include "prop.h"

#include <string.h>

void seq_clear(SeqTree *s) {
    memset(s, 0, sizeof *s);
}

int seq_from_tokens(SeqTree *s, char **tok, int ntok) {
    seq_clear(s);
    if (ntok < 1 || ntok >= SEQ_MAX) {
        return -1;
    }
    for (int i = 0; i < ntok; i++) {
        if (tok[i][0] == '\0' || tok[i][1] != '\0') {
            return -4;
        }
        char ch = tok[i][0];
        if (ch == '#') {
            continue;
        }
        if ((unsigned char)ch <= 32) {
            return -4;
        }
        s->used[i + 1] = 1;
        s->data[i + 1] = (unsigned char)ch;
        s->n++;
    }
    s->last = ntok;
    if (s->n == 0 || !s->used[1]) {
        return -3;
    }
    for (int i = 2; i <= s->last; i++) {
        if (s->used[i] && !s->used[i / 2]) {
            return -3;
        }
    }
    while (s->last > 1 && !s->used[s->last]) {
        s->last--;
    }
    return 0;
}

static int place(const BiTree *t, int id, SeqTree *s, int index) {
    if (id == 0) {
        return 0;
    }
    if (index < 1 || index >= SEQ_MAX) {
        return -1;
    }
    s->used[index] = 1;
    s->data[index] = t->node[id].data;
    if (index > s->last) {
        s->last = index;
    }
    s->n++;
    if (place(t, t->node[id].left, s, index * 2) < 0) {
        return -1;
    }
    return place(t, t->node[id].right, s, index * 2 + 1);
}

int seq_from_tree(const BiTree *t, SeqTree *s) {
    seq_clear(s);
    if (t->root == 0) {
        return 0;
    }
    return place(t, t->root, s, 1);
}

int seq_to_tree(const SeqTree *s, BiTree *t) {
    tree_init(t);
    if (s->n == 0 || !s->used[1]) {
        return -1;
    }
    int id_of[SEQ_MAX] = {0};
    for (int i = 1; i <= s->last; i++) {
        if (!s->used[i]) {
            continue;
        }
        int id = tree_alloc(t, s->data[i]);
        if (id == 0) {
            return -1;
        }
        id_of[i] = id;
    }
    for (int i = 1; i <= s->last; i++) {
        if (id_of[i] == 0) {
            continue;
        }
        int left = i * 2;
        int right = left + 1;
        if (left <= s->last) {
            t->node[id_of[i]].left = id_of[left];
        }
        if (right <= s->last) {
            t->node[id_of[i]].right = id_of[right];
        }
    }
    t->root = id_of[1];
    return 0;
}

void seq_brief(const SeqTree *s, FILE *out) {
    fprintf(out, "seq1");
    for (int i = 1; i <= s->last; i++) {
        fprintf(out, " %d:", i);
        if (s->used[i]) {
            fputc((char)s->data[i], out);
        } else {
            fputc('#', out);
        }
    }
    fprintf(out, " waste=%d\n", s->last - s->n);
}

static void put_slot(FILE *out, const SeqTree *s, int index) {
    if (index < 1 || index > s->last) {
        fputc('-', out);
        return;
    }
    fprintf(out, "%d:", index);
    fputc(s->used[index] ? (char)s->data[index] : '#', out);
}

static void put_slot0(FILE *out, const SeqTree *s, int i0) {
    if (i0 < 0 || i0 >= s->last) {
        fputc('-', out);
        return;
    }
    fprintf(out, "%d:", i0);
    fputc(s->used[i0 + 1] ? (char)s->data[i0 + 1] : '#', out);
}

static void one_slot(const SeqTree *s, int i, FILE *out) {
    int i0 = i - 1;
    fprintf(out, "i1=%d ch=%c parent1=", i, (char)s->data[i]);
    if (i == 1) {
        fputc('-', out);
    } else {
        put_slot(out, s, i / 2);
    }
    fprintf(out, " left1=");
    put_slot(out, s, i * 2);
    fprintf(out, " right1=");
    put_slot(out, s, i * 2 + 1);
    fprintf(out, " level=%d\n", prop_floor_log2(i) + 1);
    fprintf(out, "i0=%d parent0=", i0);
    if (i0 == 0) {
        fputc('-', out);
    } else {
        put_slot0(out, s, (i0 - 1) / 2);
    }
    fprintf(out, " left0=");
    put_slot0(out, s, 2 * i0 + 1);
    fprintf(out, " right0=");
    put_slot0(out, s, 2 * i0 + 2);
    fputc('\n', out);
}

void seq_relations(const SeqTree *s, FILE *out) {
    fprintf(out, "seq n=%d last=%d waste=%d\n", s->n, s->last, s->last - s->n);
    fprintf(out, "base1 child=2i,2i+1 parent=i/2\n");
    fprintf(out, "base0 child=2i+1,2i+2 parent=(i-1)/2\n");
    fprintf(out, "note 2022-41 base0 empty=-1\n");
    for (int i = 1; i <= s->last; i++) {
        if (s->used[i]) {
            one_slot(s, i, out);
        }
    }
}
