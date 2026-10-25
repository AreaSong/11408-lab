#include "run.h"

#include "bank.h"
#include "extra.h"
#include "graph.h"
#include "read.h"
#include "recover.h"

#include <stdio.h>
#include <string.h>

#define LIST_CAP 24

typedef struct {
    int loaded;
    Bank bank;
} Session;

static int fail(const char *msg) {
    printf("%s\n", msg);
    return 1;
}

static int blank(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return *s == '\0';
}

static int one_int(const char *text, int *out) {
    return parse_ints(text, out, 1);
}

static int need_state(const Session *s) {
    if (!s->loaded) {
        return fail("no-state");
    }
    return 0;
}

static int size_ok(int n, int m, int base) {
    if (n < 1 || n > BANK_N || m < 1 || m > BANK_M || (base != 0 && base != 1)) {
        return 0;
    }
    return 1;
}

static const char *row_what(int kind_need) {
    return kind_need ? "尚需" : "最大需求";
}

static int read_supply(int m, int *use_total, int *supply) {
    char line[256];
    char tok[TOK_MAX][TOK_LEN];
    int ntok;
    int i;
    int rc = read_line(line, (int)sizeof line, "请输入 avail 或 total，以及各类资源数：");
    if (rc == 1) {
        return fail("missing-line");
    }
    if (rc != 0) {
        return fail("bad-line");
    }
    if (split_tok(line, tok, TOK_MAX, &ntok) != 0 || ntok != m + 1) {
        return fail("bad-number");
    }
    if (strcmp(tok[0], "avail") == 0) {
        *use_total = 0;
    } else if (strcmp(tok[0], "total") == 0) {
        *use_total = 1;
    } else {
        return fail("bad-command");
    }
    for (i = 0; i < m; i++) {
        if (one_int(tok[i + 1], &supply[i]) != 0) {
            return fail("bad-number");
        }
    }
    return 0;
}

static int read_kind(int *kind_need) {
    char line[256];
    int rc = read_line(line, (int)sizeof line, "请输入 max 或 need：");
    if (rc == 1) {
        return fail("missing-line");
    }
    if (rc != 0) {
        return fail("bad-line");
    }
    if (strcmp(line, "max") == 0) {
        *kind_need = 0;
        return 0;
    }
    if (strcmp(line, "need") == 0) {
        *kind_need = 1;
        return 0;
    }
    return fail("bad-command");
}

static int cmd_load(Session *s, int n, int m, int base) {
    int declared[BANK_N][BANK_M];
    int supply[BANK_M];
    int use_total = 0;
    int kind_need = 0;
    Bank b;
    if (!size_ok(n, m, base)) {
        return fail("bad-size");
    }
    memset(&b, 0, sizeof b);
    b.n = n;
    b.m = m;
    b.base = base;
    if (read_supply(m, &use_total, supply) != 0) {
        return 1;
    }
    if (read_kind(&kind_need) != 0) {
        return 1;
    }
    if (read_rows(n, m, base, declared, row_what(kind_need)) != 0) {
        return 1;
    }
    if (read_rows(n, m, base, b.alloc, "已分配") != 0) {
        return 1;
    }
    if (bank_bind(&b, kind_need, declared) != 0) {
        return fail("bad-need");
    }
    if (use_total) {
        if (bank_from_total(&b, supply) != 0) {
            return fail("bad-total");
        }
    } else {
        num_copy(b.avail, supply, m);
    }
    s->bank = b;
    s->loaded = 1;
    bank_print_need(&b);
    return 0;
}

static int cmd_safe(Session *s) {
    SafeTrace trace;
    if (need_state(s) != 0) {
        return 1;
    }
    bank_safe(&s->bank, &trace);
    bank_print_trace(&s->bank, &trace);
    return 0;
}

static int cmd_count(Session *s) {
    int seqs[LIST_CAP][BANK_N];
    int total = 0;
    if (need_state(s) != 0) {
        return 1;
    }
    bank_collect(&s->bank, &total, seqs, LIST_CAP);
    bank_print_count(&s->bank, total, seqs, LIST_CAP);
    return 0;
}

static int cmd_seq(Session *s, char tok[][TOK_LEN], int ntok) {
    int order[BANK_N];
    int fail_at = -1;
    int i;
    int ok;
    const Bank *b = &s->bank;
    if (need_state(s) != 0) {
        return 1;
    }
    if (ntok != b->n + 1) {
        return fail("bad-seq");
    }
    for (i = 0; i < b->n; i++) {
        int shown;
        if (one_int(tok[i + 1], &shown) != 0) {
            return fail("bad-seq");
        }
        order[i] = shown - b->base;
    }
    ok = bank_check(b, order, &fail_at);
    if (ok < 0) {
        return fail("bad-seq");
    }
    bank_print_check(b, order, ok, fail_at);
    return 0;
}

static int cmd_request(Session *s, char tok[][TOK_LEN], int ntok) {
    int shown;
    int who;
    int req[BANK_M];
    int i;
    ReqResult result;
    const Bank *b = &s->bank;
    if (need_state(s) != 0) {
        return 1;
    }
    if (ntok != b->m + 2 || one_int(tok[1], &shown) != 0) {
        return fail("bad-number");
    }
    who = shown - b->base;
    if (who < 0 || who >= b->n) {
        return fail("bad-number");
    }
    for (i = 0; i < b->m; i++) {
        if (one_int(tok[i + 2], &req[i]) != 0) {
            return fail("bad-number");
        }
    }
    bank_request(b, who, req, &result);
    bank_print_request(b, who, req, &result);
    return 0;
}

static int fill_graph(Graph *g, int n, int m, int base) {
    int avail[BANK_M];
    char line[256];
    int rc;
    if (!size_ok(n, m, base)) {
        return fail("bad-size");
    }
    memset(g, 0, sizeof *g);
    g->n = n;
    g->m = m;
    g->base = base;
    rc = read_line(line, (int)sizeof line, "请输入可用资源：");
    if (rc == 1) {
        return fail("missing-line");
    }
    if (rc != 0) {
        return fail("bad-line");
    }
    if (parse_ints(line, avail, m) != 0) {
        return fail("bad-number");
    }
    num_copy(g->avail, avail, m);
    if (read_rows(n, m, base, g->alloc, "已分配") != 0) {
        return 1;
    }
    if (read_rows(n, m, base, g->req, "当前申请") != 0) {
        return 1;
    }
    return 0;
}

static int cmd_reduce(int n, int m, int base) {
    Graph g;
    RedTrace trace;
    if (fill_graph(&g, n, m, base) != 0) {
        return 1;
    }
    graph_reduce(&g, &trace);
    graph_print(&g, &trace);
    return 0;
}

static int cmd_recover(int n, int m, int base) {
    Graph g;
    RecResult result;
    int cost[BANK_N];
    char line[256];
    int rc;
    if (fill_graph(&g, n, m, base) != 0) {
        return 1;
    }
    rc = read_line(line, (int)sizeof line, "请输入各进程的撤销代价：");
    if (rc == 1) {
        return fail("missing-line");
    }
    if (rc != 0) {
        return fail("bad-line");
    }
    if (parse_ints(line, cost, n) != 0) {
        return fail("bad-number");
    }
    recover_run(&g, cost, &result);
    recover_print(&g, &result);
    return 0;
}

static int cmd_contrast(Session *s, int zero) {
    Graph g;
    SafeTrace safe;
    RedTrace reduced;
    if (need_state(s) != 0) {
        return 1;
    }
    memset(&g, 0, sizeof g);
    g.n = s->bank.n;
    g.m = s->bank.m;
    g.base = s->bank.base;
    num_copy(g.avail, s->bank.avail, g.m);
    memcpy(g.alloc, s->bank.alloc, sizeof g.alloc);
    if (!zero && read_rows(g.n, g.m, g.base, g.req, "当前申请") != 0) {
        return 1;
    }
    bank_safe(&s->bank, &safe);
    graph_reduce(&g, &reduced);
    printf("safety\n");
    bank_print_trace(&s->bank, &safe);
    printf("reduce\n");
    graph_print(&g, &reduced);
    return 0;
}

static int read_nmb(char tok[][TOK_LEN], int *n, int *m, int *base) {
    if (one_int(tok[1], n) || one_int(tok[2], m) || one_int(tok[3], base)) {
        return fail("bad-size");
    }
    return 0;
}

static int dispatch_state(Session *s, char tok[][TOK_LEN], int ntok) {
    if (strcmp(tok[0], "safe") == 0 && ntok == 1) {
        return cmd_safe(s);
    }
    if (strcmp(tok[0], "count") == 0 && ntok == 1) {
        return cmd_count(s);
    }
    if (strcmp(tok[0], "seq") == 0) {
        return cmd_seq(s, tok, ntok);
    }
    if (strcmp(tok[0], "request") == 0) {
        return cmd_request(s, tok, ntok);
    }
    if (strcmp(tok[0], "contrast") == 0 && ntok == 1) {
        return cmd_contrast(s, 0);
    }
    if (strcmp(tok[0], "contrast") == 0 && ntok == 2 && strcmp(tok[1], "zero") == 0) {
        return cmd_contrast(s, 1);
    }
    return -1;
}

static int dispatch(Session *s, char tok[][TOK_LEN], int ntok) {
    int n;
    int m;
    int base;
    int state;
    if (ntok < 1) {
        return 0;
    }
    if (strcmp(tok[0], "load") == 0 && ntok == 4) {
        if (read_nmb(tok, &n, &m, &base) != 0) {
            return 1;
        }
        return cmd_load(s, n, m, base);
    }
    state = dispatch_state(s, tok, ntok);
    if (state >= 0) {
        return state;
    }
    if (strcmp(tok[0], "reduce") == 0 && ntok == 4) {
        if (read_nmb(tok, &n, &m, &base) != 0) {
            return 1;
        }
        return cmd_reduce(n, m, base);
    }
    if (strcmp(tok[0], "recover") == 0 && ntok == 4) {
        if (read_nmb(tok, &n, &m, &base) != 0) {
            return 1;
        }
        return cmd_recover(n, m, base);
    }
    if (strcmp(tok[0], "phi") == 0 && ntok == 3) {
        if (one_int(tok[2], &n) != 0) {
            return fail("bad-size");
        }
        return cmd_phi(tok[1], n);
    }
    if (strcmp(tok[0], "bound") == 0 && ntok >= 2) {
        return cmd_bound(tok, ntok);
    }
    if (strcmp(tok[0], "card") == 0 && ntok == 1) {
        return cmd_card();
    }
    if (strcmp(tok[0], "claim") == 0 && ntok == 2) {
        return cmd_claim(tok[1]);
    }
    return fail("bad-command");
}

int run_loop(void) {
    Session session;
    char line[256];
    memset(&session, 0, sizeof session);
    while (1) {
        char tok[TOK_MAX][TOK_LEN];
        int ntok = 0;
        int rc = read_line(line, (int)sizeof line, "请输入命令，Ctrl+D 结束：");
        if (rc == 1) {
            return 0;
        }
        if (rc != 0) {
            return fail("bad-line");
        }
        if (blank(line)) {
            continue;
        }
        if (split_tok(line, tok, TOK_MAX, &ntok) != 0) {
            return fail("bad-command");
        }
        if (dispatch(&session, tok, ntok) != 0) {
            return 1;
        }
    }
}
