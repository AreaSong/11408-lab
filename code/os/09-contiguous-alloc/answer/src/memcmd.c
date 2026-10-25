#include "memcmd.h"

#include <stdio.h>
#include <string.h>

static const char *merge_word(int merge) {
    if (merge == 1) {
        return "front";
    }
    if (merge == 2) {
        return "back";
    }
    if (merge == 3) {
        return "both";
    }
    return "none";
}

static int map_hole(int rc) {
    if (rc == -1) {
        return fail("overlap");
    }
    if (rc == -2) {
        return fail("full");
    }
    if (rc == -4) {
        return fail("has-memory");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    return 0;
}

static int cmd_algo(App *app, char tok[][TOK_W], int ntok) {
    Algo algo;
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (algo_parse(tok[1], &algo) != 0) {
        return fail("bad-command");
    }
    space_begin(&app->space, algo, app->bind_dynamic);
    app->has_space = 1;
    printf("algo=%s\n", algo_name(algo));
    return 0;
}

static int cmd_memory(App *app, char tok[][TOK_W], int ntok) {
    Algo algo;
    long long cap = 0;
    int rc;
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (algo_parse(tok[1], &algo) != 0) {
        return fail("bad-command");
    }
    rc = read_size(tok[2], &cap);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0 || space_memory(&app->space, algo, cap, app->bind_dynamic) != 0) {
        return fail("bad-size");
    }
    app->has_space = 1;
    printf("memory=%lld algo=%s\n", cap, algo_name(algo));
    return 0;
}

static int cmd_hole(App *app, char tok[][TOK_W], int ntok) {
    long long start = 0;
    long long size = 0;
    int rc;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_u(tok[1], &start) != 0) {
        return fail("bad-number");
    }
    rc = read_size(tok[2], &size);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    rc = space_add_hole(&app->space, start, size);
    if (map_hole(rc) != 0) {
        return 1;
    }
    printf("hole start=%lld size=%lld\n", start, size);
    return 0;
}

static int cmd_alloc(App *app, char tok[][TOK_W], int ntok) {
    AllocInfo info;
    long long size = 0;
    int rc;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    rc = read_size(tok[2], &size);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    rc = space_alloc(&app->space, tok[1], size, &info);
    if (rc == -3) {
        return fail("duplicate");
    }
    if (rc == -4) {
        return fail("full");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    if (!info.ok) {
        printf("alloc %s size=%lld result=fail free=%lld largest=%lld external=%lld\n",
               tok[1], size, info.free_sum, info.largest, info.external);
        return 0;
    }
    printf("alloc %s size=%lld result=ok at=%lld from=%lld hole=%lld left=%lld internal=0\n",
           tok[1], size, info.at, info.from, info.hole, info.left);
    return 0;
}

static void print_merge(const char *word, const char *name, const FreeInfo *info) {
    if (name != 0) {
        printf("%s %s at=%lld size=%lld merge=%s", word, name, info->at, info->size,
               merge_word(info->merge));
    } else {
        printf("%s at=%lld size=%lld merge=%s", word, info->at, info->size,
               merge_word(info->merge));
    }
    if (info->merge != 0) {
        printf(" into=%lld into-size=%lld", info->into, info->into_size);
    }
    printf("\n");
}

static int cmd_free(App *app, char tok[][TOK_W], int ntok) {
    FreeInfo info;
    int rc;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 2)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    rc = space_free_name(&app->space, tok[1], &info);
    if (rc == -1) {
        return fail("no-job");
    }
    if (rc != 0) {
        return fail("full");
    }
    print_merge("free", tok[1], &info);
    return 0;
}

static int cmd_release(App *app, char tok[][TOK_W], int ntok) {
    FreeInfo info;
    long long start = 0;
    long long size = 0;
    int rc;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_u(tok[1], &start) != 0) {
        return fail("bad-number");
    }
    rc = read_size(tok[2], &size);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    rc = space_release(&app->space, start, size, &info);
    if (map_hole(rc) != 0) {
        return 1;
    }
    print_merge("release", 0, &info);
    return 0;
}

static int cmd_show(App *app, char tok[][TOK_W], int ntok) {
    Hole holes[BLOCK_MAX];
    (void)tok;
    Job jobs[BLOCK_MAX];
    long long sum = 0;
    long long largest = 0;
    long long cursor = 0;
    int nh;
    int nj;
    int i;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 1)) {
        return 1;
    }
    nh = space_chain(&app->space, holes, BLOCK_MAX);
    nj = space_jobs(&app->space, jobs, BLOCK_MAX);
    space_stat(&app->space, &sum, &largest);
    printf("show algo=%s order=%s holes=%d free=%lld largest=%lld cursor=",
           algo_name(app->space.algo), algo_order(app->space.algo), nh, sum, largest);
    if (space_cursor(&app->space, &cursor) == 0) {
        printf("%lld\n", cursor);
    } else {
        printf("-\n");
    }
    for (i = 0; i < nh; i++) {
        printf("%lld %lld\n", holes[i].start, holes[i].size);
    }
    printf("jobs=%d\n", nj);
    for (i = 0; i < nj; i++) {
        printf("%s %lld %lld\n", jobs[i].name, jobs[i].start, jobs[i].size);
    }
    return 0;
}

static int cmd_compact(App *app, char tok[][TOK_W], int ntok) {
    Move moves[BLOCK_MAX];
    (void)tok;
    long long free_at = 0;
    long long free_size = 0;
    int nmove = 0;
    int rc;
    int i;
    if (!app->has_space) {
        return fail("no-space");
    }
    if (need_args(ntok, 1)) {
        return 1;
    }
    rc = space_compact(&app->space, moves, BLOCK_MAX, &nmove, &free_at, &free_size);
    if (rc == -1) {
        printf("compact result=fail reason=hole-only\n");
        return 0;
    }
    if (rc == -2) {
        printf("compact result=fail reason=static\n");
        return 0;
    }
    if (rc != 0) {
        return fail("full");
    }
    printf("compact result=ok reloc=dynamic\n");
    for (i = 0; i < nmove; i++) {
        printf("%s %lld %lld -> %lld\n", moves[i].name, moves[i].old_at, moves[i].size,
               moves[i].new_at);
    }
    if (free_size > 0) {
        printf("free %lld %lld\n", free_at, free_size);
    } else {
        printf("free none\n");
    }
    return 0;
}

int cmd_mem(App *app, char tok[][TOK_W], int ntok) {
    const char *cmd = tok[0];
    if (strcmp(cmd, "algo") == 0) {
        return cmd_algo(app, tok, ntok);
    }
    if (strcmp(cmd, "memory") == 0) {
        return cmd_memory(app, tok, ntok);
    }
    if (strcmp(cmd, "hole") == 0) {
        return cmd_hole(app, tok, ntok);
    }
    if (strcmp(cmd, "alloc") == 0) {
        return cmd_alloc(app, tok, ntok);
    }
    if (strcmp(cmd, "free") == 0) {
        return cmd_free(app, tok, ntok);
    }
    if (strcmp(cmd, "release") == 0) {
        return cmd_release(app, tok, ntok);
    }
    if (strcmp(cmd, "show") == 0) {
        return cmd_show(app, tok, ntok);
    }
    if (strcmp(cmd, "compact") == 0) {
        return cmd_compact(app, tok, ntok);
    }
    return fail("bad-command");
}
