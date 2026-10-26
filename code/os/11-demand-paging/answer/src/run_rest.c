#include "run.h"

#include "alloc.h"
#include "exam.h"
#include "fetch.h"
#include "origin.h"
#include "slot.h"

#include <stdio.h>
#include <string.h>

static int cmd_fetch(char tok[][TOK_W], int ntok) {
    unsigned long long head[3];
    int refs[24];
    int nref;
    int i;
    int faults = 0;
    int prefetched = 0;
    int ahead;

    if (ntok < 6 || ntok > 5 + 24) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "demand") != 0 && strcmp(tok[1], "prep") != 0) {
        return fail("bad-command");
    }
    if (read_args(tok, 2, 3, head)) {
        return 1;
    }
    if (head[0] < 1 || head[0] > FETCH_FRAMES || head[1] > 8 || head[2] < 1 || head[2] > 64) {
        return fail("bad-number");
    }
    ahead = (int)head[1];
    if (strcmp(tok[1], "demand") == 0 && ahead != 0) {
        return fail("bad-number");
    }
    if (strcmp(tok[1], "prep") == 0 && (ahead < 1 || ahead > 8)) {
        return fail("bad-number");
    }
    nref = ntok - 5;
    for (i = 0; i < nref; i++) {
        unsigned long long page = 0;
        if (read_ull(tok[5 + i], &page) != 0 || page > 100000) {
            return fail("bad-number");
        }
        refs[i] = (int)page;
    }
    if (fetch_run((int)head[0], ahead, (int)head[2], refs, nref, &faults, &prefetched) != 0) {
        return fail("bad-number");
    }
    printf("fetch mode=%s frames=%llu ahead=%d space=%llu faults=%d prefetched=%d\n", tok[1], head[0], ahead,
           head[2], faults, prefetched);
    return 0;
}

static int cmd_origin(char tok[][TOK_W], int ntok) {
    Origin origin;

    if (need_args(ntok, 3)) {
        return 1;
    }
    if (origin_of(tok[1], tok[2], &origin) != 0) {
        return fail("bad-command");
    }
    printf("origin mode=%s kind=%s from=%s evict=%s\n", tok[1], tok[2], origin.from, origin.evict);
    return 0;
}

static int cmd_split(char tok[][TOK_W], int ntok) {
    unsigned long long num[2];
    unsigned long long page = 0;
    unsigned long long offset = 0;

    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_args(tok, 1, 2, num)) {
        return 1;
    }
    if (addr_split(num[0], num[1], &page, &offset) != 0) {
        return fail("bad-number");
    }
    printf("split page-bytes=");
    print_hex(num[0]);
    printf(" addr=");
    print_hex(num[1]);
    printf(" page=");
    print_hex(page);
    printf(" offset=");
    print_hex(offset);
    printf("\n");
    return 0;
}

static int cmd_pte(char tok[][TOK_W], int ntok) {
    unsigned long long num[4];
    unsigned long long addr = 0;

    if (need_args(ntok, 5)) {
        return 1;
    }
    if (read_args(tok, 1, 4, num)) {
        return 1;
    }
    if (pte_addr(num[0], num[1], num[2], num[3], &addr) != 0) {
        return fail("bad-number");
    }
    printf("pte base=");
    print_hex(num[2]);
    printf(" page=");
    print_hex(num[3]);
    printf(" addr=");
    print_hex(addr);
    printf("\n");
    return 0;
}

static int cmd_combo(char tok[][TOK_W], int ntok) {
    int legal = 0;

    if (need_args(ntok, 2)) {
        return 1;
    }
    if (alloc_combo(tok[1], &legal) != 0) {
        return fail("bad-policy");
    }
    printf("combo name=%s result=%s\n", tok[1], legal ? "ok" : "illegal");
    return 0;
}

int cmd_rest(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "fetch") == 0) {
        return cmd_fetch(tok, ntok);
    }
    if (strcmp(tok[0], "origin") == 0) {
        return cmd_origin(tok, ntok);
    }
    if (strcmp(tok[0], "split") == 0) {
        return cmd_split(tok, ntok);
    }
    if (strcmp(tok[0], "pte") == 0) {
        return cmd_pte(tok, ntok);
    }
    if (strcmp(tok[0], "combo") == 0) {
        return cmd_combo(tok, ntok);
    }
    if (strcmp(tok[0], "card") == 0) {
        if (need_args(ntok, 1)) {
            return 1;
        }
        card_print();
        return 0;
    }
    if (strcmp(tok[0], "exam") == 0) {
        if (need_args(ntok, 2)) {
            return 1;
        }
        if (exam_print(tok[1]) != 0) {
            return fail("bad-exam");
        }
        return 0;
    }
    if (strcmp(tok[0], "claim") == 0) {
        if (need_args(ntok, 2)) {
            return 1;
        }
        if (claim_print(tok[1]) != 0) {
            return fail("bad-claim");
        }
        return 0;
    }
    return fail("bad-command");
}
