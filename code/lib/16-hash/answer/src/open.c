#include "hash.h"

static void snap(const HashTab *t, int p, OpenWalk *w) {
    w->pos[w->npath] = p;
    w->cell_state[w->npath] = t->state[p];
    w->cell_key[w->npath] = t->key[p];
    w->npath++;
}

static int place(HashTab *t, int at, int key, int cmp, OpenWalk *w) {
    t->state[at] = CELL_FULL;
    t->key[at] = key;
    t->n++;
    w->put = at;
    w->cmp = cmp;
    w->rc = HASH_OK;
    return HASH_OK;
}

static int seq_of(const HashTab *t, int key, int *h, int *seq) {
    int step = 1;
    *h = hash_addr(t, key);
    if (t->kind == KIND_DOUBLE) {
        step = hash_step(t, key);
    }
    return hash_probe(t, *h, step, seq);
}

/*
 * cmp 是以后查找这个关键字的比较次数，停在它实际所在的那一格。
 * 插入时如果前面有删除标记，仍要继续走到空位，确认后面没有同一个关键字，
 * 所以路径可能比 cmp 长。空位才是查找失败的终点，删除标记不能当成空位。
 */
int open_insert(HashTab *t, int key, OpenWalk *w) {
    int seq[HASH_M_MAX];
    int h = 0;
    int len = seq_of(t, key, &h, seq);
    int tomb = -1;
    int tomb_i = -1;
    int i;
    w->h = h;
    w->npath = 0;
    w->put = -1;
    w->cmp = 0;
    for (i = 0; i < len; i++) {
        int p = seq[i];
        snap(t, p, w);
        if (t->state[p] == CELL_EMPTY) {
            int at = tomb >= 0 ? tomb : p;
            int cmp = tomb >= 0 ? tomb_i + 1 : i + 1;
            return place(t, at, key, cmp, w);
        }
        if (t->state[p] == CELL_TOMB && tomb < 0) {
            tomb = p;
            tomb_i = i;
        } else if (t->state[p] == CELL_FULL && t->key[p] == key) {
            w->put = p;
            w->cmp = i + 1;
            w->rc = HASH_DUP;
            return HASH_DUP;
        }
    }
    if (tomb >= 0) {
        return place(t, tomb, key, tomb_i + 1, w);
    }
    w->rc = HASH_FULL;
    w->cmp = w->npath;
    return HASH_FULL;
}

int open_find(const HashTab *t, int key, OpenWalk *w) {
    int seq[HASH_M_MAX];
    int h = 0;
    int len = seq_of(t, key, &h, seq);
    int i;
    w->h = h;
    w->npath = 0;
    w->put = -1;
    w->cmp = 0;
    for (i = 0; i < len; i++) {
        int p = seq[i];
        snap(t, p, w);
        if (t->state[p] == CELL_EMPTY) {
            w->put = p;
            w->cmp = i + 1;
            w->rc = HASH_MISS;
            return HASH_MISS;
        }
        if (t->state[p] == CELL_FULL && t->key[p] == key) {
            w->put = p;
            w->cmp = i + 1;
            w->rc = HASH_OK;
            return HASH_OK;
        }
    }
    w->cmp = len;
    w->rc = HASH_MISS;
    return HASH_MISS;
}

int open_delete(HashTab *t, int key, int wipe, OpenWalk *w) {
    int rc = open_find(t, key, w);
    if (rc != HASH_OK) {
        w->rc = HASH_MISS;
        return HASH_MISS;
    }
    /* 直接置空会截断探测链。删除标记让后面的关键字仍能被找到。 */
    t->state[w->put] = wipe ? CELL_EMPTY : CELL_TOMB;
    t->n--;
    w->rc = HASH_OK;
    return HASH_OK;
}
