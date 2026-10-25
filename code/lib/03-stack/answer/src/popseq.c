#include "popseq.h"

#include <stdlib.h>
#include <string.h>

#define POP_ENUM_CAP 1430

long long popseq_catalan(int n) {
    long long c = 1;
    if (n < 0 || n > POP_COUNT_MAX) {
        return -1;
    }
    /* C_n = C_{n-1} * (4n-2) / (n+1)，从 C_0 = 1 推。每一步都能整除。 */
    for (int i = 1; i <= n; i++) {
        c = c * (4LL * i - 2) / (i + 1);
    }
    return c;
}

static long long dfs_count(int n, int depth, int next, int produced) {
    long long ways = 0;
    if (produced == n) {
        return 1;
    }
    if (next <= n) {
        ways += dfs_count(n, depth + 1, next + 1, produced);
    }
    if (depth > 0) {
        ways += dfs_count(n, depth - 1, next, produced + 1);
    }
    return ways;
}

long long popseq_count(int n) {
    if (n < 1 || n > POP_COUNT_MAX) {
        return -1;
    }
    return dfs_count(n, 0, 1, 0);
}

static void print_ints(FILE *out, const int *st, int top) {
    fputc('[', out);
    for (int i = 0; i <= top; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fprintf(out, "%d", st[i]);
    }
    fputc(']', out);
}

bool popseq_check(int n, const int *seq, FILE *out) {
    int st[POP_CHECK_MAX];
    int top = -1;
    int next = 1;
    if (n < 1 || n > POP_CHECK_MAX || seq == NULL) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        int want = seq[i];
        if (out != NULL) {
            fprintf(out, "want=%d\n", want);
        }
        while (top < 0 || st[top] != want) {
            if (next > n) {
                if (out != NULL) {
                    if (top < 0) {
                        fprintf(out, "stuck empty\n");
                    } else {
                        fprintf(out, "stuck top=%d\n", st[top]);
                    }
                    fprintf(out, "legal=0\n");
                }
                return false;
            }
            st[++top] = next++;
            if (out != NULL) {
                fprintf(out, "push=%d stack=", st[top]);
                print_ints(out, st, top);
                fputc('\n', out);
            }
        }
        if (out != NULL) {
            fprintf(out, "pop=%d stack=", st[top]);
        }
        top--;
        if (out != NULL) {
            print_ints(out, st, top);
            fputc('\n', out);
        }
    }
    if (out != NULL) {
        fprintf(out, "legal=1\n");
    }
    return true;
}

static void dfs_save(int n, int *st, int top, int next, int *cur, int len,
                     int (*bag)[POP_ENUM_MAX], int *count) {
    if (len == n) {
        if (*count < POP_ENUM_CAP) {
            memcpy(bag[*count], cur, sizeof(int) * (size_t)n);
            (*count)++;
        }
        return;
    }
    if (next <= n) {
        st[++top] = next;
        dfs_save(n, st, top, next + 1, cur, len, bag, count);
        top--;
    }
    if (top >= 0) {
        int value = st[top];
        top--;
        cur[len] = value;
        dfs_save(n, st, top, next, cur, len + 1, bag, count);
        st[++top] = value;
    }
}

static int seq_before(const int *a, const int *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return 0;
}

static void sort_seqs(int (*bag)[POP_ENUM_MAX], int count, int n) {
    for (int i = 1; i < count; i++) {
        int key[POP_ENUM_MAX];
        int j = i;
        memcpy(key, bag[i], sizeof(int) * (size_t)n);
        while (j > 0 && seq_before(key, bag[j - 1], n)) {
            memcpy(bag[j], bag[j - 1], sizeof(int) * (size_t)n);
            j--;
        }
        memcpy(bag[j], key, sizeof(int) * (size_t)n);
    }
}

static int collect(int n, int (*bag)[POP_ENUM_MAX]) {
    int st[POP_ENUM_MAX];
    int cur[POP_ENUM_MAX];
    int count = 0;
    dfs_save(n, st, -1, 1, cur, 0, bag, &count);
    sort_seqs(bag, count, n);
    return count;
}

static void print_seq(FILE *out, const int *seq, int n) {
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fprintf(out, "%d", seq[i]);
    }
    fputc('\n', out);
}

static int (*alloc_bag(void))[POP_ENUM_MAX] {
    return calloc((size_t)POP_ENUM_CAP, sizeof(int[POP_ENUM_MAX]));
}

int popseq_enum(int n, FILE *out) {
    int (*bag)[POP_ENUM_MAX] = NULL;
    int count = 0;
    long long cat = 0;
    if (n < 1 || n > POP_ENUM_MAX) {
        return -1;
    }
    bag = alloc_bag();
    if (bag == NULL) {
        return -1;
    }
    count = collect(n, bag);
    cat = popseq_catalan(n);
    if (out != NULL) {
        for (int i = 0; i < count; i++) {
            print_seq(out, bag[i], n);
        }
        fprintf(out, "count=%d catalan=%lld match=%d\n", count, cat, (long long)count == cat);
    }
    free(bag);
    return count;
}

int popseq_prefix(int n, int first, FILE *out) {
    int (*bag)[POP_ENUM_MAX] = NULL;
    int count = 0;
    int shown = 0;
    if (n < 1 || n > POP_ENUM_MAX) {
        return -1;
    }
    bag = alloc_bag();
    if (bag == NULL) {
        return -1;
    }
    count = collect(n, bag);
    for (int i = 0; i < count; i++) {
        if (bag[i][0] != first) {
            continue;
        }
        if (out != NULL) {
            print_seq(out, bag[i], n);
        }
        shown++;
    }
    if (out != NULL) {
        fprintf(out, "count=%d\n", shown);
    }
    free(bag);
    return shown;
}
