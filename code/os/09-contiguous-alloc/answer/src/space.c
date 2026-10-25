#include "space.h"

#include <string.h>

static void copy_name(char *dst, const char *src) {
    size_t i = 0;
    while (i < 15 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int hit(long long a, long long as, long long b, long long bs) {
    return a < b + bs && b < a + as;
}

static int insert_hole(Space *s, long long start, long long size) {
    int i = 0;
    if (s->nh >= BLOCK_MAX) {
        return -1;
    }
    while (i < s->nh && s->holes[i].start < start) {
        i++;
    }
    memmove(&s->holes[i + 1], &s->holes[i], (size_t)(s->nh - i) * sizeof(Hole));
    s->holes[i].start = start;
    s->holes[i].size = size;
    s->nh++;
    return 0;
}

static void remove_hole(Space *s, int index) {
    memmove(&s->holes[index], &s->holes[index + 1],
            (size_t)(s->nh - index - 1) * sizeof(Hole));
    s->nh--;
}

static int find_front(const Space *s, long long start) {
    int i;
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].start + s->holes[i].size == start) {
            return i;
        }
    }
    return -1;
}

static int find_back(const Space *s, long long end) {
    int i;
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].start == end) {
            return i;
        }
    }
    return -1;
}

static int find_job(const Space *s, const char *name) {
    int i;
    for (i = 0; i < s->nj; i++) {
        if (strcmp(s->jobs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void remove_job(Space *s, int index) {
    memmove(&s->jobs[index], &s->jobs[index + 1],
            (size_t)(s->nj - index - 1) * sizeof(Job));
    s->nj--;
}

static void insert_job(Space *s, const char *name, long long start, long long size) {
    int i = 0;
    while (i < s->nj && s->jobs[i].start < start) {
        i++;
    }
    memmove(&s->jobs[i + 1], &s->jobs[i], (size_t)(s->nj - i) * sizeof(Job));
    s->jobs[i].start = start;
    s->jobs[i].size = size;
    copy_name(s->jobs[i].name, name);
    s->nj++;
}

/* 链序：i 应该排在 j 前面时返回 1。地址序是存储序，容量序只用于选择和显示。 */
static int hole_before(const Space *s, int i, int j) {
    long long si = s->holes[i].size;
    long long sj = s->holes[j].size;
    if (s->algo == ALGO_BF) {
        if (si != sj) {
            return si < sj;
        }
        return s->holes[i].start < s->holes[j].start;
    }
    if (s->algo == ALGO_WF) {
        if (si != sj) {
            return si > sj;
        }
        return s->holes[i].start < s->holes[j].start;
    }
    return s->holes[i].start < s->holes[j].start;
}

static int cursor_index(const Space *s) {
    int i;
    if (!s->cursor_valid || s->nh == 0) {
        return 0;
    }
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].start >= s->cursor) {
            return i;
        }
    }
    return 0;
}

static void cursor_after_exact(Space *s, long long removed) {
    int i;
    if (s->nh == 0) {
        s->cursor_valid = 0;
        return;
    }
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].start > removed) {
            s->cursor = s->holes[i].start;
            s->cursor_valid = 1;
            return;
        }
    }
    s->cursor = s->holes[0].start;
    s->cursor_valid = 1;
}

static void retarget(Space *s, long long old) {
    int i;
    if (s->nh == 0) {
        s->cursor_valid = 0;
        return;
    }
    for (i = 0; i < s->nh; i++) {
        long long st = s->holes[i].start;
        long long sz = s->holes[i].size;
        if (st == old || (st <= old && old < st + sz)) {
            s->cursor = st;
            s->cursor_valid = 1;
            return;
        }
    }
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].start >= old) {
            s->cursor = s->holes[i].start;
            s->cursor_valid = 1;
            return;
        }
    }
    s->cursor = s->holes[0].start;
    s->cursor_valid = 1;
}

static void point_low(Space *s) {
    if (s->nh == 0) {
        s->cursor_valid = 0;
        return;
    }
    s->cursor = s->holes[0].start;
    s->cursor_valid = 1;
}

static int pick_hole(const Space *s, long long need) {
    int i;
    int best;
    int start;
    int n;
    if (s->nh == 0) {
        return -1;
    }
    if (s->algo == ALGO_NF) {
        start = cursor_index(s);
        for (n = 0; n < s->nh; n++) {
            i = (start + n) % s->nh;
            if (s->holes[i].size >= need) {
                return i;
            }
        }
        return -1;
    }
    best = -1;
    for (i = 0; i < s->nh; i++) {
        if (s->holes[i].size < need) {
            continue;
        }
        if (best < 0 || hole_before(s, i, best)) {
            best = i;
        }
    }
    return best;
}

/* 从低地址切。剩下的高地址部分仍是空闲，不是分给进程的内部碎片。 */
static void take_hole(Space *s, int pick, long long need, AllocInfo *out) {
    Hole h = s->holes[pick];
    out->ok = 1;
    out->at = h.start;
    out->from = h.start;
    out->hole = h.size;
    out->left = h.size - need;
    out->external = 0;
    if (out->left == 0) {
        remove_hole(s, pick);
        if (s->algo == ALGO_NF) {
            cursor_after_exact(s, h.start);
        }
    } else {
        s->holes[pick].start = h.start + need;
        s->holes[pick].size = out->left;
        if (s->algo == ALGO_NF) {
            s->cursor = h.start + need;
            s->cursor_valid = 1;
        }
    }
    s->allocated_once = 1;
}

static void drop_merged(Space *s, int front, int back) {
    int hi;
    int lo;
    if (front >= 0 && back >= 0) {
        hi = front > back ? front : back;
        lo = front > back ? back : front;
        remove_hole(s, hi);
        remove_hole(s, lo);
        return;
    }
    if (front >= 0) {
        remove_hole(s, front);
    } else if (back >= 0) {
        remove_hole(s, back);
    }
}

static int range_busy(const Space *s, long long start, long long size) {
    int i;
    for (i = 0; i < s->nh; i++) {
        if (hit(start, size, s->holes[i].start, s->holes[i].size)) {
            return 1;
        }
    }
    for (i = 0; i < s->nj; i++) {
        if (hit(start, size, s->jobs[i].start, s->jobs[i].size)) {
            return 1;
        }
    }
    return 0;
}

static int merge_range(Space *s, long long start, long long size, FreeInfo *out) {
    int front;
    int back;
    int kind = 0;
    long long ns = start;
    long long nz = size;
    long long old_cursor;
    if (size <= 0 || start < 0) {
        return -3;
    }
    if (s->has_mem && start + size > s->mem) {
        return -3;
    }
    if (range_busy(s, start, size)) {
        return -1;
    }
    front = find_front(s, start);
    back = find_back(s, start + size);
    if (front < 0 && back < 0 && s->nh >= BLOCK_MAX) {
        return -2;
    }
    if (front >= 0) {
        ns = s->holes[front].start;
        nz += s->holes[front].size;
        kind = 1;
    }
    if (back >= 0) {
        nz += s->holes[back].size;
        kind = kind == 1 ? 3 : 2;
    }
    drop_merged(s, front, back);
    if (insert_hole(s, ns, nz) != 0) {
        return -2;
    }
    out->merge = kind;
    out->at = start;
    out->size = size;
    out->into = ns;
    out->into_size = nz;
    old_cursor = s->cursor;
    if (s->algo == ALGO_NF) {
        if (s->allocated_once) {
            retarget(s, old_cursor);
        } else {
            point_low(s);
        }
    }
    return 0;
}

int algo_parse(const char *text, Algo *out) {
    if (strcmp(text, "ff") == 0) {
        *out = ALGO_FF;
    } else if (strcmp(text, "bf") == 0) {
        *out = ALGO_BF;
    } else if (strcmp(text, "wf") == 0) {
        *out = ALGO_WF;
    } else if (strcmp(text, "nf") == 0) {
        *out = ALGO_NF;
    } else {
        return -1;
    }
    return 0;
}

const char *algo_name(Algo algo) {
    if (algo == ALGO_BF) {
        return "bf";
    }
    if (algo == ALGO_WF) {
        return "wf";
    }
    if (algo == ALGO_NF) {
        return "nf";
    }
    return "ff";
}

const char *algo_order(Algo algo) {
    if (algo == ALGO_BF) {
        return "size-asc";
    }
    if (algo == ALGO_WF) {
        return "size-desc";
    }
    return "address-asc";
}

void space_begin(Space *s, Algo algo, int dynamic) {
    memset(s, 0, sizeof *s);
    s->algo = algo;
    s->bind_dynamic = dynamic ? 1 : 0;
}

int space_memory(Space *s, Algo algo, long long cap, int dynamic) {
    if (cap <= 0) {
        return -3;
    }
    space_begin(s, algo, dynamic);
    s->has_mem = 1;
    s->mem = cap;
    s->holes[0].start = 0;
    s->holes[0].size = cap;
    s->nh = 1;
    s->cursor = 0;
    s->cursor_valid = 1;
    return 0;
}

void space_set_bind(Space *s, int dynamic) {
    s->bind_dynamic = dynamic ? 1 : 0;
}

int space_add_hole(Space *s, long long start, long long size) {
    FreeInfo info;
    if (s->has_mem) {
        return -4;
    }
    /* 初始链里两块空闲如果刚好接上，本来就该是同一块。 */
    return merge_range(s, start, size, &info);
}

int space_alloc(Space *s, const char *name, long long need, AllocInfo *out) {
    int pick;
    memset(out, 0, sizeof *out);
    if (need <= 0) {
        return -2;
    }
    if (find_job(s, name) >= 0) {
        return -3;
    }
    if (s->nj >= BLOCK_MAX) {
        return -4;
    }
    pick = pick_hole(s, need);
    if (pick < 0) {
        space_stat(s, &out->free_sum, &out->largest);
        out->external = out->free_sum;
        return 0;
    }
    take_hole(s, pick, need, out);
    insert_job(s, name, out->at, need);
    return 0;
}

int space_free_name(Space *s, const char *name, FreeInfo *out) {
    int index = find_job(s, name);
    long long start;
    long long size;
    if (index < 0) {
        return -1;
    }
    start = s->jobs[index].start;
    size = s->jobs[index].size;
    if (find_front(s, start) < 0 && find_back(s, start + size) < 0 && s->nh >= BLOCK_MAX) {
        return -2;
    }
    remove_job(s, index);
    return merge_range(s, start, size, out);
}

int space_release(Space *s, long long start, long long size, FreeInfo *out) {
    if (s->has_mem) {
        return -4;
    }
    return merge_range(s, start, size, out);
}

int space_compact(Space *s, Move *moves, int cap, int *nmove, long long *free_at,
                  long long *free_size) {
    int i;
    long long pos = 0;
    if (!s->has_mem) {
        return -1;
    }
    if (!s->bind_dynamic) {
        return -2;
    }
    if (s->nj > cap) {
        return -3;
    }
    for (i = 0; i < s->nj; i++) {
        moves[i].old_at = s->jobs[i].start;
        moves[i].size = s->jobs[i].size;
        copy_name(moves[i].name, s->jobs[i].name);
        s->jobs[i].start = pos;
        moves[i].new_at = pos;
        pos += moves[i].size;
    }
    *nmove = s->nj;
    s->nh = 0;
    if (pos < s->mem) {
        s->holes[0].start = pos;
        s->holes[0].size = s->mem - pos;
        s->nh = 1;
        *free_at = pos;
        *free_size = s->mem - pos;
    } else {
        *free_at = 0;
        *free_size = 0;
    }
    if (s->algo == ALGO_NF) {
        point_low(s);
        s->allocated_once = 1;
    }
    return 0;
}

int space_chain(const Space *s, Hole *out, int cap) {
    int idx[BLOCK_MAX];
    int i;
    int j;
    int copy;
    for (i = 0; i < s->nh; i++) {
        idx[i] = i;
    }
    for (i = 1; i < s->nh; i++) {
        int key = idx[i];
        j = i;
        while (j > 0 && hole_before(s, key, idx[j - 1])) {
            idx[j] = idx[j - 1];
            j--;
        }
        idx[j] = key;
    }
    copy = s->nh < cap ? s->nh : cap;
    for (i = 0; i < copy; i++) {
        out[i] = s->holes[idx[i]];
    }
    return s->nh;
}
