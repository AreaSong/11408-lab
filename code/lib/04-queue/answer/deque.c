/*
 * 输入受限、输出受限，以及“先全部入队再从左端出队”的 batch。
 * 输入顺序是 1..n。队头在左。
 */
#include <stdio.h>
#include <string.h>

#include "deque.h"
#include "prompt.h"

static int is_perm(int n, const int *seq) {
    char seen[DEQUE_BATCH_MAX + 1];
    memset(seen, 0, (size_t)n + 1);
    for (int i = 0; i < n; i++) {
        if (seq[i] < 1 || seq[i] > n || seen[seq[i]]) {
            return 0;
        }
        seen[seq[i]] = 1;
    }
    return 1;
}

static void print_seq(int n, const int *seq) {
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            putchar(' ');
        }
        printf("%d", seq[i]);
    }
}

/* 0：输入结束或残缺。1：已经打印过失败，继续下一条。2：序列可用。 */
static int read_seq(int maxn, int *n, int *seq) {
    if (scanf("%d", n) != 1) {
        return 0;
    }
    if (*n < 1 || *n > maxn) {
        printf("fail n=%d\n", *n);
        return 1;
    }
    for (int i = 0; i < *n; i++) {
        if (scanf("%d", &seq[i]) != 1) {
            return 0;
        }
    }
    if (!is_perm(*n, seq)) {
        printf("fail seq\n");
        return 1;
    }
    return 2;
}

static int begin_case(const char *cmd, int maxn, int *n, int *seq) {
    int rc = read_seq(maxn, n, seq);
    if (rc != 2) {
        return rc;
    }
    printf("case %s n=%d seq=", cmd, *n);
    print_seq(*n, seq);
    printf("\n");
    return 2;
}

static int cmd_batch(void) {
    int seq[DEQUE_BATCH_MAX];
    int n = 0;
    int rc = begin_case("batch", DEQUE_BATCH_MAX, &n, seq);
    if (rc == 2) {
        deque_batch_check(n, seq, stdout);
        return 1;
    }
    return rc;
}

static int cmd_in(void) {
    int seq[DEQUE_CHECK_MAX];
    int n = 0;
    int rc = begin_case("in", DEQUE_CHECK_MAX, &n, seq);
    if (rc == 2) {
        deque_in_check(n, seq, stdout);
        return 1;
    }
    return rc;
}

static int cmd_out(void) {
    int seq[DEQUE_CHECK_MAX];
    int n = 0;
    int rc = begin_case("out", DEQUE_CHECK_MAX, &n, seq);
    if (rc == 2) {
        deque_out_check(n, seq, stdout);
        return 1;
    }
    return rc;
}

static int cmd_class(void) {
    int seq[DEQUE_CHECK_MAX];
    int n = 0;
    int rc = begin_case("class", DEQUE_CHECK_MAX, &n, seq);
    if (rc != 2) {
        return rc;
    }
    printf("in=%d out=%d batch=%d\n", deque_in_check(n, seq, NULL) ? 1 : 0,
           deque_out_check(n, seq, NULL) ? 1 : 0, deque_batch_check(n, seq, NULL) ? 1 : 0);
    return 1;
}

static int cmd_enum(void) {
    int n = 0;
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    if (deque_enum(n, stdout) < 0) {
        printf("fail n=%d\n", n);
    }
    return 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "batch") == 0) {
        return cmd_batch();
    }
    if (strcmp(cmd, "in") == 0) {
        return cmd_in();
    }
    if (strcmp(cmd, "out") == 0) {
        return cmd_out();
    }
    if (strcmp(cmd, "class") == 0) {
        return cmd_class();
    }
    if (strcmp(cmd, "enum") == 0) {
        return cmd_enum();
    }
    printf("fail unknown %s\n", cmd);
    return 1;
}

int main(void) {
    char cmd[16];
    for (;;) {
        prompt("请输入命令（batch / in / out / class / enum，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            return 0;
        }
        if (!dispatch(cmd)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
    }
}
