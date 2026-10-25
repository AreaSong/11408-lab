#include "partcmd.h"

#include <stdio.h>
#include <string.h>

static int need_ready(int ready, const char *msg) {
    if (!ready) {
        return fail(msg);
    }
    return 0;
}

static int read_name_size(char tok[][TOK_W], int ntok, long long *size) {
    int rc;
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    rc = read_size(tok[2], size);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    return 0;
}

static int cmd_fixed(App *app, char tok[][TOK_W], int ntok) {
    (void)tok;
    if (need_args(ntok, 1)) {
        return 1;
    }
    fixed_begin(&app->fixed);
    app->has_fixed = 1;
    printf("fixed\n");
    return 0;
}

static int cmd_slot(App *app, char tok[][TOK_W], int ntok) {
    long long start = 0;
    long long size = 0;
    int rc;
    if (need_ready(app->has_fixed, "no-fixed") || need_args(ntok, 3)) {
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
    rc = fixed_slot(&app->fixed, start, size);
    if (rc == -1) {
        return fail("overlap");
    }
    if (rc == -2) {
        return fail("full");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    printf("slot start=%lld size=%lld\n", start, size);
    return 0;
}

static int cmd_fput(App *app, char tok[][TOK_W], int ntok) {
    FixedPut info;
    long long size = 0;
    int rc;
    if (need_ready(app->has_fixed, "no-fixed") || read_name_size(tok, ntok, &size)) {
        return 1;
    }
    rc = fixed_put(&app->fixed, tok[1], size, &info);
    if (rc == -3) {
        return fail("duplicate");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    if (!info.ok) {
        printf("fput %s need=%lld result=fail reason=no-partition sum=%lld external=0\n",
               tok[1], size, info.sum);
        return 0;
    }
    printf("fput %s need=%lld result=ok slot=%lld size=%lld internal=%lld sum=%lld\n",
           tok[1], size, info.slot, info.slot_size, info.internal, info.sum);
    return 0;
}

static int cmd_fdrop(App *app, char tok[][TOK_W], int ntok) {
    if (need_ready(app->has_fixed, "no-fixed") || need_args(ntok, 2)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    if (fixed_drop(&app->fixed, tok[1]) != 0) {
        return fail("no-job");
    }
    printf("fdrop %s sum=%lld\n", tok[1], fixed_sum(&app->fixed));
    return 0;
}

static int cmd_fshow(App *app, char tok[][TOK_W], int ntok) {
    int i;
    (void)tok;
    if (need_ready(app->has_fixed, "no-fixed") || need_args(ntok, 1)) {
        return 1;
    }
    printf("fshow slots=%d sum=%lld external=0\n", app->fixed.n, fixed_sum(&app->fixed));
    for (i = 0; i < app->fixed.n; i++) {
        const Slot *slot = &app->fixed.slots[i];
        if (slot->used) {
            printf("%lld %lld %s internal=%lld\n", slot->start, slot->size, slot->name,
                   slot->size - slot->need);
        } else {
            printf("%lld %lld - free\n", slot->start, slot->size);
        }
    }
    return 0;
}

static int cmd_single(App *app, char tok[][TOK_W], int ntok) {
    long long user = 0;
    int rc;
    if (need_args(ntok, 2)) {
        return 1;
    }
    rc = read_size(tok[1], &user);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0 || single_begin(&app->single, user) != 0) {
        return fail("bad-size");
    }
    app->has_single = 1;
    printf("single user=%lld\n", user);
    return 0;
}

static int cmd_sput(App *app, char tok[][TOK_W], int ntok) {
    SinglePut info;
    long long size = 0;
    int rc;
    if (need_ready(app->has_single, "no-single") || read_name_size(tok, ntok, &size)) {
        return 1;
    }
    rc = single_put(&app->single, tok[1], size, &info);
    if (rc != 0) {
        return fail("bad-size");
    }
    if (info.busy) {
        printf("sput %s need=%lld result=fail reason=busy\n", tok[1], size);
        return 0;
    }
    if (info.too_big) {
        printf("sput %s need=%lld result=fail reason=too-big\n", tok[1], size);
        return 0;
    }
    printf("sput %s need=%lld result=ok internal=%lld external=0\n", tok[1], size,
           info.internal);
    return 0;
}

static int cmd_sdrop(App *app, char tok[][TOK_W], int ntok) {
    if (need_ready(app->has_single, "no-single") || need_args(ntok, 2)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    if (single_drop(&app->single, tok[1]) != 0) {
        return fail("no-job");
    }
    printf("sdrop %s result=ok\n", tok[1]);
    return 0;
}

static int cmd_buddy(App *app, char tok[][TOK_W], int ntok) {
    long long mem = 0;
    int rc;
    if (need_args(ntok, 2)) {
        return 1;
    }
    rc = read_size(tok[1], &mem);
    if (rc == -1) {
        return fail("bad-number");
    }
    if (rc != 0 || buddy_open(&app->buddy, mem) != 0) {
        return fail("bad-size");
    }
    app->has_buddy = 1;
    printf("buddy mem=%lld\n", mem);
    return 0;
}

static int cmd_balloc(App *app, char tok[][TOK_W], int ntok) {
    BuddyPut info;
    long long size = 0;
    int rc;
    if (need_ready(app->has_buddy, "no-buddy") || read_name_size(tok, ntok, &size)) {
        return 1;
    }
    rc = buddy_alloc(&app->buddy, tok[1], size, &info);
    if (rc == -3) {
        return fail("duplicate");
    }
    if (rc == -4) {
        return fail("full");
    }
    if (rc != 0) {
        return fail("bad-size");
    }
    if (!info.ok && info.reason == 1) {
        printf("balloc %s need=%lld result=fail reason=too-big\n", tok[1], size);
        return 0;
    }
    if (!info.ok) {
        printf("balloc %s need=%lld result=fail reason=no-block\n", tok[1], size);
        return 0;
    }
    printf("balloc %s need=%lld result=ok size=%lld at=%lld\n", tok[1], size, info.size,
           info.at);
    return 0;
}

static int cmd_bfree(App *app, char tok[][TOK_W], int ntok) {
    int merges = 0;
    if (need_ready(app->has_buddy, "no-buddy") || need_args(ntok, 2)) {
        return 1;
    }
    if (!good_name(tok[1])) {
        return fail("bad-name");
    }
    if (buddy_free(&app->buddy, tok[1], &merges) != 0) {
        return fail("no-job");
    }
    printf("bfree %s merges=%d\n", tok[1], merges);
    return 0;
}

static int cmd_bshow(App *app, char tok[][TOK_W], int ntok) {
    BuddyBlk blocks[BUDDY_MAX];
    (void)tok;
    int n;
    int i;
    if (need_ready(app->has_buddy, "no-buddy") || need_args(ntok, 1)) {
        return 1;
    }
    n = buddy_collect(&app->buddy, 1, blocks, BUDDY_MAX);
    printf("bshow free=%d\n", n);
    for (i = 0; i < n; i++) {
        printf("%lld %lld\n", blocks[i].start, blocks[i].size);
    }
    n = buddy_collect(&app->buddy, 0, blocks, BUDDY_MAX);
    printf("jobs=%d\n", n);
    for (i = 0; i < n; i++) {
        printf("%s %lld %lld\n", blocks[i].name, blocks[i].start, blocks[i].size);
    }
    return 0;
}

int cmd_part(App *app, char tok[][TOK_W], int ntok) {
    const char *cmd = tok[0];
    if (strcmp(cmd, "fixed") == 0) {
        return cmd_fixed(app, tok, ntok);
    }
    if (strcmp(cmd, "slot") == 0) {
        return cmd_slot(app, tok, ntok);
    }
    if (strcmp(cmd, "fput") == 0) {
        return cmd_fput(app, tok, ntok);
    }
    if (strcmp(cmd, "fdrop") == 0) {
        return cmd_fdrop(app, tok, ntok);
    }
    if (strcmp(cmd, "fshow") == 0) {
        return cmd_fshow(app, tok, ntok);
    }
    if (strcmp(cmd, "single") == 0) {
        return cmd_single(app, tok, ntok);
    }
    if (strcmp(cmd, "sput") == 0) {
        return cmd_sput(app, tok, ntok);
    }
    if (strcmp(cmd, "sdrop") == 0) {
        return cmd_sdrop(app, tok, ntok);
    }
    if (strcmp(cmd, "buddy") == 0) {
        return cmd_buddy(app, tok, ntok);
    }
    if (strcmp(cmd, "balloc") == 0) {
        return cmd_balloc(app, tok, ntok);
    }
    if (strcmp(cmd, "bfree") == 0) {
        return cmd_bfree(app, tok, ntok);
    }
    if (strcmp(cmd, "bshow") == 0) {
        return cmd_bshow(app, tok, ntok);
    }
    return fail("bad-command");
}
