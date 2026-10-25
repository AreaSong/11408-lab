#include "run.h"

#include "exam.h"
#include "io.h"
#include "memcmd.h"
#include "partcmd.h"
#include "reloc.h"

#include <stdio.h>
#include <string.h>

static int cmd_base(App *app, char tok[][TOK_W], int ntok) {
    long long base = 0;
    long long limit = 0;
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_u(tok[1], &base) != 0 || read_u(tok[2], &limit) != 0) {
        return fail("bad-number");
    }
    app->has_base = 1;
    app->base = base;
    app->limit = limit;
    printf("base=%lld limit=%lld\n", base, limit);
    return 0;
}

static int cmd_logical(App *app, char tok[][TOK_W], int ntok) {
    long long addr = 0;
    long long physical = 0;
    int ok = 0;
    if (!app->has_base) {
        return fail("no-base");
    }
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (read_u(tok[1], &addr) != 0) {
        return fail("bad-number");
    }
    if (reloc_logical(app->base, app->limit, addr, &ok, &physical) != 0) {
        return fail("bad-number");
    }
    if (ok) {
        printf("logical=%lld base=%lld limit=%lld result=ok physical=%lld\n", addr, app->base,
               app->limit, physical);
    } else {
        printf("logical=%lld base=%lld limit=%lld result=bound\n", addr, app->base, app->limit);
    }
    return 0;
}

static int cmd_bounds(App *app, char tok[][TOK_W], int ntok) {
    long long low = 0;
    long long high = 0;
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_u(tok[1], &low) != 0 || read_u(tok[2], &high) != 0) {
        return fail("bad-number");
    }
    if (low > high) {
        return fail("bad-register");
    }
    app->has_bounds = 1;
    app->low = low;
    app->high = high;
    printf("low=%lld high=%lld\n", low, high);
    return 0;
}

static int cmd_physical(App *app, char tok[][TOK_W], int ntok) {
    long long addr = 0;
    int ok = 0;
    if (!app->has_bounds) {
        return fail("no-bounds");
    }
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (read_u(tok[1], &addr) != 0) {
        return fail("bad-number");
    }
    if (reloc_bounds(app->low, app->high, addr, &ok) != 0) {
        return fail("bad-register");
    }
    if (ok) {
        printf("physical=%lld low=%lld high=%lld result=ok\n", addr, app->low, app->high);
    } else {
        printf("physical=%lld low=%lld high=%lld result=bound\n", addr, app->low, app->high);
    }
    return 0;
}

static int cmd_bind(App *app, char tok[][TOK_W], int ntok) {
    int dynamic;
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (strcmp(tok[1], "dynamic") == 0) {
        dynamic = 1;
    } else if (strcmp(tok[1], "static") == 0) {
        dynamic = 0;
    } else {
        return fail("bad-command");
    }
    app->bind_dynamic = dynamic;
    if (app->has_space) {
        space_set_bind(&app->space, dynamic);
    }
    printf("bind=%s\n", dynamic ? "dynamic" : "static");
    return 0;
}

static int cmd_exam(App *app, char tok[][TOK_W], int ntok) {
    (void)app;
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (exam_print(tok[1]) != 0) {
        return fail("bad-exam");
    }
    return 0;
}

static int cmd_card(App *app, char tok[][TOK_W], int ntok) {
    (void)app;
    (void)tok;
    if (need_args(ntok, 1)) {
        return 1;
    }
    card_print();
    return 0;
}

static int cmd_other(App *app, char tok[][TOK_W], int ntok) {
    const char *cmd = tok[0];
    if (strcmp(cmd, "base") == 0) {
        return cmd_base(app, tok, ntok);
    }
    if (strcmp(cmd, "logical") == 0) {
        return cmd_logical(app, tok, ntok);
    }
    if (strcmp(cmd, "bounds") == 0) {
        return cmd_bounds(app, tok, ntok);
    }
    if (strcmp(cmd, "physical") == 0) {
        return cmd_physical(app, tok, ntok);
    }
    if (strcmp(cmd, "bind") == 0) {
        return cmd_bind(app, tok, ntok);
    }
    if (strcmp(cmd, "exam") == 0) {
        return cmd_exam(app, tok, ntok);
    }
    if (strcmp(cmd, "card") == 0) {
        return cmd_card(app, tok, ntok);
    }
    return fail("bad-command");
}

static int route(App *app, char tok[][TOK_W], int ntok) {
    const char *cmd = tok[0];
    if (strcmp(cmd, "base") == 0 || strcmp(cmd, "logical") == 0 || strcmp(cmd, "bounds") == 0 ||
        strcmp(cmd, "physical") == 0 || strcmp(cmd, "bind") == 0 || strcmp(cmd, "exam") == 0 ||
        strcmp(cmd, "card") == 0) {
        return cmd_other(app, tok, ntok);
    }
    if (strcmp(cmd, "algo") == 0 || strcmp(cmd, "memory") == 0 || strcmp(cmd, "hole") == 0 ||
        strcmp(cmd, "alloc") == 0 || strcmp(cmd, "free") == 0 || strcmp(cmd, "release") == 0 ||
        strcmp(cmd, "show") == 0 || strcmp(cmd, "compact") == 0) {
        return cmd_mem(app, tok, ntok);
    }
    if (strcmp(cmd, "fixed") == 0 || strcmp(cmd, "slot") == 0 || strcmp(cmd, "fput") == 0 ||
        strcmp(cmd, "fdrop") == 0 || strcmp(cmd, "fshow") == 0 || strcmp(cmd, "single") == 0 ||
        strcmp(cmd, "sput") == 0 || strcmp(cmd, "sdrop") == 0 || strcmp(cmd, "buddy") == 0 ||
        strcmp(cmd, "balloc") == 0 || strcmp(cmd, "bfree") == 0 || strcmp(cmd, "bshow") == 0) {
        return cmd_part(app, tok, ntok);
    }
    return fail("bad-command");
}

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

int run_loop(void) {
    App app;
    char line[256];
    char tok[TOK_N][TOK_W];
    memset(&app, 0, sizeof app);
    app.bind_dynamic = 1;
    for (;;) {
        int ntok = 0;
        int rc = read_cmd(line, (int)sizeof line);
        if (rc == 1) {
            return 0;
        }
        if (rc != 0) {
            return 1;
        }
        if (skipped(line)) {
            continue;
        }
        if (split_toks(line, tok, &ntok) != 0) {
            return fail("bad-command");
        }
        if (route(&app, tok, ntok) != 0) {
            return 1;
        }
    }
}
