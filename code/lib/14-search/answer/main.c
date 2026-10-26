#include "bin.h"
#include "blk.h"
#include "prompt.h"
#include "seq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 4096
#define TOK_MAX 80

enum { TAB_NONE = 0, TAB_SEQ, TAB_BIN, TAB_BLK };

typedef struct {
    int which;
    SeqTable seq;
    BinTable bin;
    BlkTable blk;
} App;

static void say(int rc) {
    const char *name = "range";
    if (rc == S_OK) {
        return;
    }
    if (rc == S_ORDER) {
        name = "order";
    } else if (rc == S_EMPTY) {
        name = "empty";
    } else if (rc == S_DIV) {
        name = "divide";
    } else if (rc == S_FULL) {
        name = "full";
    }
    printf("fail %s\n", name);
}

static int read_line(char *buf, int cap) {
    int c;
    if (fgets(buf, cap, stdin) == NULL) {
        return 0;
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        while ((c = getchar()) != EOF && c != '\n') {
        }
        return -1;
    }
    return 1;
}

static int split(char *line, char **tok, int cap) {
    int n = 0;
    char *p = line;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p == '\0' || *p == '\n') {
            break;
        }
        if (n >= cap) {
            return -1;
        }
        tok[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p == '\n') {
            *p = '\0';
            break;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return n;
}

/* 1 合法；0 不是整数；-1 超出关键字允许的范围。 */
static int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v;
    if (s == NULL || s[0] == '\0') {
        return 0;
    }
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0;
    }
    if (v < KEY_LO || v > KEY_HI) {
        return -1;
    }
    *out = (int)v;
    return 1;
}

static int gather(char **tok, int ntok, int from, int *dst, int cap, int *nout) {
    int i;
    int n = 0;
    for (i = from; i < ntok; i++) {
        int v;
        int rc;
        if (n >= cap) {
            return -2;
        }
        rc = parse_int(tok[i], &v);
        if (rc == 0) {
            return 0;
        }
        if (rc < 0) {
            return -1;
        }
        dst[n++] = v;
    }
    *nout = n;
    return 1;
}

static int need_table(const App *app) {
    if (app->which == TAB_NONE) {
        printf("fail empty\n");
        return 0;
    }
    return 1;
}

static void do_seq(App *app, char **tok, int ntok) {
    int kind;
    int keys[TABLE_MAX];
    int nk = 0;
    int rc;
    if (ntok < 2) {
        printf("fail format\n");
        return;
    }
    kind = seq_kind(tok[1]);
    if (kind < 0) {
        printf("fail format\n");
        return;
    }
    rc = gather(tok, ntok, 2, keys, TABLE_MAX, &nk);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    rc = seq_load(&app->seq, kind, keys, nk);
    if (rc != S_OK) {
        say(rc);
        return;
    }
    app->which = TAB_SEQ;
    seq_report(&app->seq, stdout);
}

static void do_prob(App *app, char **tok, int ntok) {
    int w[TABLE_MAX];
    int nw = 0;
    int rc;
    if (!need_table(app)) {
        return;
    }
    if (app->which == TAB_BLK) {
        printf("fail kind\n");
        return;
    }
    rc = gather(tok, ntok, 1, w, TABLE_MAX, &nw);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    if (app->which == TAB_SEQ) {
        rc = seq_weight(&app->seq, w, nw);
    } else {
        rc = bin_weight(&app->bin, w, nw);
    }
    if (rc != S_OK) {
        say(rc);
        return;
    }
    if (app->which == TAB_SEQ) {
        seq_print_asl(&app->seq, stdout);
    } else {
        bin_print_asl(&app->bin, stdout);
    }
}

static void do_bin(App *app, char **tok, int ntok) {
    int mode;
    int keys[TABLE_MAX];
    int nk = 0;
    int rc;
    if (ntok < 2) {
        printf("fail format\n");
        return;
    }
    mode = bin_mode(tok[1]);
    if (mode < 0) {
        printf("fail format\n");
        return;
    }
    rc = gather(tok, ntok, 2, keys, TABLE_MAX, &nk);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    rc = bin_load(&app->bin, mode, keys, nk);
    if (rc != S_OK) {
        say(rc);
        return;
    }
    app->which = TAB_BIN;
    bin_report(&app->bin, stdout);
}

static void do_find(App *app, char **tok, int ntok) {
    int key = 0;
    int cmp = 0;
    int nk = 0;
    int rc;
    if (!need_table(app)) {
        return;
    }
    if (app->which == TAB_BLK && app->blk.b == 0) {
        printf("fail empty\n");
        return;
    }
    rc = gather(tok, ntok, 1, &key, 1, &nk);
    if (rc == 0 || rc == -2) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    if (nk != 1) {
        printf("fail format\n");
        return;
    }
    printf("find %d\n", key);
    if (app->which == TAB_SEQ) {
        seq_find(&app->seq, key, stdout, &cmp);
    } else if (app->which == TAB_BIN) {
        bin_find(&app->bin, key, stdout, &cmp);
    } else {
        blk_find(&app->blk, key, stdout, &cmp);
    }
}

static void do_asl(App *app) {
    if (!need_table(app)) {
        return;
    }
    if (app->which == TAB_SEQ) {
        seq_print_asl(&app->seq, stdout);
    } else if (app->which == TAB_BIN) {
        bin_print_asl(&app->bin, stdout);
    } else if (app->blk.b == 0) {
        printf("fail empty\n");
    } else {
        blk_print_asl(&app->blk, stdout);
    }
}

static void do_tree(const App *app) {
    if (!need_table(app)) {
        return;
    }
    if (app->which != TAB_BIN) {
        printf("fail kind\n");
        return;
    }
    bin_print_tree(&app->bin, stdout);
}

static void do_show(const App *app) {
    if (!need_table(app)) {
        return;
    }
    if (app->which != TAB_BLK) {
        printf("fail kind\n");
        return;
    }
    blk_show(&app->blk, stdout);
}

static void do_worst(char **tok, int ntok) {
    int mode;
    int n = 0;
    int nk = 0;
    int rc;
    if (ntok != 3) {
        printf("fail format\n");
        return;
    }
    mode = bin_mode(tok[1]);
    if (mode < 0) {
        printf("fail format\n");
        return;
    }
    rc = gather(tok, ntok, 2, &n, 1, &nk);
    if (rc == 0 || rc == -2) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    if (nk != 1) {
        printf("fail format\n");
        return;
    }
    say(bin_worst(n, mode, stdout));
}

static void do_path(char **tok, int ntok) {
    int keys[TABLE_MAX];
    int nk = 0;
    int rc = gather(tok, ntok, 1, keys, TABLE_MAX, &nk);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    if (nk < 1) {
        printf("fail format\n");
        return;
    }
    printf("path\n");
    bin_path(keys, nk, stdout);
}

static void do_fit(char **tok, int ntok) {
    if (ntok != 3 || bin_suitable(tok[1], tok[2]) < 0) {
        printf("fail format\n");
        return;
    }
    bin_fit(tok[1], tok[2], stdout);
}

static void do_blk(App *app, char **tok, int ntok) {
    int index_bin;
    int inner_bin;
    if (ntok != 3) {
        printf("fail format\n");
        return;
    }
    index_bin = blk_mode(tok[1]);
    inner_bin = blk_mode(tok[2]);
    if (index_bin < 0 || inner_bin < 0) {
        printf("fail format\n");
        return;
    }
    blk_begin(&app->blk, index_bin, inner_bin);
    app->which = TAB_BLK;
    printf("blk index=%s inner=%s\n", index_bin ? "bin" : "seq", inner_bin ? "bin" : "seq");
}

static void do_add(App *app, char **tok, int ntok) {
    int keys[BLOCK_LEN];
    int nk = 0;
    int rc;
    if (!need_table(app)) {
        return;
    }
    if (app->which != TAB_BLK) {
        printf("fail kind\n");
        return;
    }
    rc = gather(tok, ntok, 1, keys, BLOCK_LEN, &nk);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    say(blk_add(&app->blk, keys, nk));
}

static void do_best(char **tok, int ntok) {
    BestAsk ask;
    int nums[BEST_NS + 1];
    int nnum = 0;
    int rc;
    if (ntok < 4) {
        printf("fail format\n");
        return;
    }
    ask.index_bin = blk_mode(tok[1]);
    ask.inner_bin = blk_mode(tok[2]);
    if (ask.index_bin < 0 || ask.inner_bin < 0) {
        printf("fail format\n");
        return;
    }
    rc = gather(tok, ntok, 3, nums, BEST_NS + 1, &nnum);
    if (rc == 0) {
        printf("fail format\n");
        return;
    }
    if (rc < 0) {
        printf("fail range\n");
        return;
    }
    if (nnum < 2) {
        printf("fail format\n");
        return;
    }
    ask.n = nums[0];
    ask.sizes = nums + 1;
    ask.ns = nnum - 1;
    say(blk_best(&ask, stdout));
}

static void dispatch(App *app, char **tok, int ntok) {
    if (ntok == 0) {
        return;
    }
    if (strcmp(tok[0], "seq") == 0) {
        do_seq(app, tok, ntok);
    } else if (strcmp(tok[0], "prob") == 0) {
        do_prob(app, tok, ntok);
    } else if (strcmp(tok[0], "bin") == 0) {
        do_bin(app, tok, ntok);
    } else if (strcmp(tok[0], "find") == 0) {
        do_find(app, tok, ntok);
    } else if (strcmp(tok[0], "asl") == 0 && ntok == 1) {
        do_asl(app);
    } else if (strcmp(tok[0], "tree") == 0 && ntok == 1) {
        do_tree(app);
    } else if (strcmp(tok[0], "show") == 0 && ntok == 1) {
        do_show(app);
    } else if (strcmp(tok[0], "worst") == 0) {
        do_worst(tok, ntok);
    } else if (strcmp(tok[0], "path") == 0) {
        do_path(tok, ntok);
    } else if (strcmp(tok[0], "fit") == 0) {
        do_fit(tok, ntok);
    } else if (strcmp(tok[0], "blk") == 0) {
        do_blk(app, tok, ntok);
    } else if (strcmp(tok[0], "add") == 0) {
        do_add(app, tok, ntok);
    } else if (strcmp(tok[0], "best") == 0) {
        do_best(tok, ntok);
    } else if (strcmp(tok[0], "asl") == 0 || strcmp(tok[0], "tree") == 0 || strcmp(tok[0], "show") == 0) {
        printf("fail format\n");
    } else {
        printf("fail unknown\n");
    }
}

int main(void) {
    App app;
    char line[LINE_MAX];
    memset(&app, 0, sizeof app);
    for (;;) {
        char *tok[TOK_MAX];
        int ntok;
        int rc;
        prompt("请输入命令（seq / bin / blk / add / find / asl / prob / tree / show / worst / path / fit / "
               "best，一行一条，Ctrl+D 结束）：");
        rc = read_line(line, (int)sizeof line);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            printf("fail long\n");
            continue;
        }
        ntok = split(line, tok, TOK_MAX);
        if (ntok < 0) {
            printf("fail format\n");
            continue;
        }
        dispatch(&app, tok, ntok);
    }
    return 0;
}
