#include "cmd.h"

#include "eat.h"
#include "seg.h"
#include "trace.h"
#include "trans.h"

#include <stdio.h>
#include <string.h>

static Trace session;
static int session_ready;

static void print_fraction(unsigned long long num, unsigned long long den) {
    if (den == 1) {
        printf("%llu", num);
    } else {
        printf("%llu/%llu", num, den);
    }
}

static int as_flag(unsigned long long value, int *out) {
    if (value > 1) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static int as_walks(unsigned long long value, int *out) {
    if (value < 1 || value > 8) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static const char *flat_name(int result) {
    if (result == FLAT_BOUND) {
        return "bound";
    }
    if (result == FLAT_FAULT) {
        return "fault";
    }
    return "ok";
}

static int cmd_flat(char tok[][TOK_W], int ntok) {
    unsigned long long a[8];
    FlatInfo info;
    int valid = 0;

    if (need_args(ntok, 9) || read_args(tok, 1, 8, a)) {
        return 1;
    }
    if (as_flag(a[5], &valid) != 0) {
        return fail("bad-number");
    }
    if (trans_flat(a[0], a[1], a[2], a[3], a[4], valid, a[6], a[7], &info) != 0) {
        return fail("bad-number");
    }
    printf("flat page=%llu result=%s", a[4], flat_name(info.result));
    if (info.result != FLAT_BOUND) {
        printf(" pte=");
        print_hex(info.pte);
    }
    if (info.result == FLAT_OK) {
        printf(" frame=");
        print_hex(a[6]);
        printf(" pa=");
        print_hex(info.pa);
    }
    printf("\n");
    return 0;
}

static const char *two_name(int result) {
    if (result == TWO_FAULT_DIR) {
        return "fault-dir";
    }
    if (result == TWO_FAULT_PAGE) {
        return "fault-page";
    }
    return "ok";
}

static int cmd_two(char tok[][TOK_W], int ntok) {
    unsigned long long a[10];
    TwoInfo info;
    int dir_valid = 0;
    int page_valid = 0;

    if (need_args(ntok, 11) || read_args(tok, 1, 10, a)) {
        return 1;
    }
    if (as_flag(a[4], &dir_valid) != 0 || as_flag(a[7], &page_valid) != 0) {
        return fail("bad-number");
    }
    if (trans_two(a[0], a[1], a[2], a[3], dir_valid, a[5], a[6], page_valid, a[8], a[9], &info) != 0) {
        return fail("bad-number");
    }
    printf("two result=%s pde=", two_name(info.result));
    print_hex(info.pde);
    if (info.result != TWO_FAULT_DIR) {
        printf(" pt-base=");
        print_hex(info.pt_base);
        printf(" pte=");
        print_hex(info.pte);
    }
    if (info.result == TWO_OK) {
        printf(" pa=");
        print_hex(info.pa);
    }
    printf("\n");
    return 0;
}

static int cmd_look(char tok[][TOK_W], int ntok) {
    unsigned long long a[8];
    LookInfo info;
    int tlb_valid = 0;
    int pt_valid = 0;
    const char *name = "fault";

    if (need_args(ntok, 9) || read_args(tok, 1, 8, a)) {
        return 1;
    }
    if (as_flag(a[3], &tlb_valid) != 0 || as_flag(a[6], &pt_valid) != 0) {
        return fail("bad-number");
    }
    if (trans_look(a[0], a[1], a[2], tlb_valid, a[4], a[5], pt_valid, a[7], &info) != 0) {
        return fail("bad-number");
    }
    if (info.result == LOOK_TLB) {
        name = "tlb-hit";
    } else if (info.result == LOOK_PT) {
        name = "pt-hit";
    }
    printf("look result=%s", name);
    if (info.result != LOOK_FAULT) {
        printf(" frame=");
        print_hex(info.frame);
        printf(" pa=");
        print_hex(info.pa);
    }
    printf("\n");
    return 0;
}

static int cmd_avg(char tok[][TOK_W], int ntok) {
    unsigned long long a[5];
    unsigned long long num = 0;
    unsigned long long den = 0;
    int walks = 0;

    if (need_args(ntok, 6) || read_args(tok, 1, 5, a)) {
        return 1;
    }
    if (as_walks(a[4], &walks) != 0) {
        return fail("bad-number");
    }
    if (eat_avg(a[0], a[1], a[2], a[3], walks, &num, &den) != 0) {
        return fail("bad-number");
    }
    printf("avg hit=");
    print_fraction(a[0], a[1]);
    printf(" walks=%d eat=", walks);
    print_fraction(num, den);
    printf("\n");
    return 0;
}

static int cmd_rate(char tok[][TOK_W], int ntok) {
    unsigned long long a[4];
    unsigned long long num = 0;
    unsigned long long den = 0;

    if (need_args(ntok, 5) || read_args(tok, 1, 4, a)) {
        return 1;
    }
    if (eat_rate(a[0], a[1], a[2], a[3], &num, &den) != 0) {
        return fail("bad-number");
    }
    printf("rate ok=");
    print_fraction(a[0], a[1]);
    printf(" eat=");
    print_fraction(num, den);
    printf("\n");
    return 0;
}

static int cmd_path(char tok[][TOK_W], int ntok) {
    unsigned long long ns = 0;
    unsigned long long a[4];
    int walks = 0;
    const char *kind;

    if (ntok < 2) {
        return fail("bad-command");
    }
    kind = tok[1];
    if (strcmp(kind, "tlb-hit") == 0) {
        if (need_args(ntok, 4) || read_args(tok, 2, 2, a)) {
            return 1;
        }
        if (eat_tlb_hit(a[0], a[1], &ns) != 0) {
            return fail("bad-number");
        }
    } else if (strcmp(kind, "pt-hit") == 0) {
        if (need_args(ntok, 5) || read_args(tok, 2, 3, a)) {
            return 1;
        }
        if (as_walks(a[2], &walks) != 0 || eat_pt_hit(a[0], a[1], walks, &ns) != 0) {
            return fail("bad-number");
        }
    } else if (strcmp(kind, "fault") == 0) {
        if (need_args(ntok, 6) || read_args(tok, 2, 4, a)) {
            return 1;
        }
        if (as_walks(a[2], &walks) != 0 || eat_fault(a[0], a[1], walks, a[3], &ns) != 0) {
            return fail("bad-number");
        }
    } else {
        return fail("bad-command");
    }
    printf("path kind=%s ns=%llu\n", kind, ns);
    return 0;
}

static int cmd_seg(char tok[][TOK_W], int ntok) {
    unsigned long long a[3];
    unsigned long long pa = 0;
    int result = 0;

    if (need_args(ntok, 4) || read_args(tok, 1, 3, a)) {
        return 1;
    }
    if (seg_translate(a[0], a[1], a[2], &result, &pa) != 0) {
        return fail("bad-number");
    }
    printf("seg offset=%llu result=%s", a[2], result == SEG_OK ? "ok" : "bound");
    if (result == SEG_OK) {
        printf(" pa=");
        print_hex(pa);
    }
    printf("\n");
    return 0;
}

static int cmd_seglen(char tok[][TOK_W], int ntok) {
    unsigned long long a[2];
    unsigned long long segments = 0;
    unsigned long long max_len = 0;

    if (need_args(ntok, 3) || read_args(tok, 1, 2, a)) {
        return 1;
    }
    if (a[0] > 63 || a[1] > 63) {
        return fail("bad-number");
    }
    if (seg_len((int)a[0], (int)a[1], &segments, &max_len) != 0) {
        return fail("bad-number");
    }
    printf("seglen segments=%llu max=%llu\n", segments, max_len);
    return 0;
}

static const char *segno_name(int result) {
    if (result == SEG_BOUND_NO) {
        return "bound-seg";
    }
    if (result == SEG_BOUND_OFF) {
        return "bound-off";
    }
    return "ok";
}

static int cmd_segno(char tok[][TOK_W], int ntok) {
    unsigned long long a[5];
    unsigned long long pa = 0;
    int result = 0;

    if (need_args(ntok, 6) || read_args(tok, 1, 5, a)) {
        return 1;
    }
    if (seg_check(a[0], a[1], a[2], a[3], a[4], &result, &pa) != 0) {
        return fail("bad-number");
    }
    printf("segno result=%s", segno_name(result));
    if (result == SEG_OK) {
        printf(" pa=");
        print_hex(pa);
    }
    printf("\n");
    return 0;
}

static const char *sp_name(int result) {
    if (result == SP_BOUND) {
        return "bound";
    }
    if (result == SP_FAULT) {
        return "fault";
    }
    if (result == SP_TLB) {
        return "tlb-hit";
    }
    return "pt-hit";
}

static int cmd_segpage(char tok[][TOK_W], int ntok) {
    unsigned long long a[7];
    SegPageInfo info;
    int valid = 0;
    int tlb = 0;
    int rc;

    if (need_args(ntok, 8) || read_args(tok, 1, 7, a)) {
        return 1;
    }
    if (as_flag(a[4], &valid) != 0 || as_flag(a[6], &tlb) != 0) {
        return fail("bad-number");
    }
    rc = seg_page(a[0], a[1], a[2], a[3], valid, a[5], tlb, &info);
    if (rc == -2) {
        return fail("bad-tlb");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    printf("segpage result=%s", sp_name(info.result));
    if (info.result == SP_TLB || info.result == SP_PT) {
        printf(" pa=");
        print_hex(info.pa);
    }
    printf(" visits=%d\n", info.visits);
    return 0;
}

static int cmd_trace_reset(char tok[][TOK_W], int ntok) {
    unsigned long long a[5];
    Trace next;

    if (need_args(ntok, 7) || read_args(tok, 2, 5, a)) {
        return 1;
    }
    if (a[4] < 1 || a[4] > TRACE_PAGES) {
        return fail("bad-size");
    }
    if (trace_init(&next, a[0], a[1], a[2], a[3], (int)a[4]) != 0) {
        return fail("bad-size");
    }
    session = next;
    session_ready = 1;
    printf("trace page=%llu tlb=%llu mem=%llu fault=%llu resident=%d slots=%d\n", a[0], a[1], a[2], a[3],
           session.resident, TRACE_TLB);
    return 0;
}

static int map_error(int rc) {
    if (rc == -1) {
        return fail("duplicate");
    }
    if (rc == -2) {
        return fail("full");
    }
    if (rc == -3) {
        return fail("over-resident");
    }
    return fail("bad-number");
}

static int cmd_trace_map(char tok[][TOK_W], int ntok) {
    unsigned long long a[3];
    int valid = 0;
    int rc;

    if (!session_ready) {
        return fail("no-trace");
    }
    if (need_args(ntok, 5) || read_args(tok, 2, 3, a)) {
        return 1;
    }
    if (as_flag(a[2], &valid) != 0) {
        return fail("bad-number");
    }
    rc = trace_map(&session, a[0], a[1], valid);
    if (rc != 0) {
        return map_error(rc);
    }
    printf("map page=%llu frame=", a[0]);
    print_hex(a[1]);
    printf(" valid=%d\n", valid);
    return 0;
}

static const char *go_name(int result) {
    if (result == GO_TLB) {
        return "tlb-hit";
    }
    if (result == GO_PT) {
        return "pt-hit";
    }
    if (result == GO_FAULT) {
        return "fault";
    }
    return "bound";
}

static int cmd_trace_go(char tok[][TOK_W], int ntok) {
    unsigned long long addr = 0;
    GoInfo info;

    if (!session_ready) {
        return fail("no-trace");
    }
    if (need_args(ntok, 3)) {
        return 1;
    }
    if (read_ull(tok[2], &addr) != 0) {
        return fail("bad-number");
    }
    if (trace_go(&session, addr, &info) != 0) {
        return fail("bad-number");
    }
    printf("go addr=");
    print_hex(addr);
    printf(" page=%llu offset=", info.page);
    print_hex(info.offset);
    printf(" result=%s", go_name(info.result));
    if (info.result != GO_BOUND) {
        printf(" frame=");
        print_hex(info.frame);
        printf(" pa=");
        print_hex(info.pa);
    }
    printf(" ns=%llu victim=", info.ns);
    if (info.victim < 0) {
        printf("-");
    } else {
        printf("%lld", info.victim);
    }
    printf("\n");
    return 0;
}

static int cmd_trace(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "reset") == 0) {
        return cmd_trace_reset(tok, ntok);
    }
    if (strcmp(tok[1], "map") == 0) {
        return cmd_trace_map(tok, ntok);
    }
    if (strcmp(tok[1], "go") == 0) {
        return cmd_trace_go(tok, ntok);
    }
    return fail("bad-command");
}

int cmd_walk(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "flat") == 0) {
        return cmd_flat(tok, ntok);
    }
    if (strcmp(tok[0], "two") == 0) {
        return cmd_two(tok, ntok);
    }
    if (strcmp(tok[0], "look") == 0) {
        return cmd_look(tok, ntok);
    }
    if (strcmp(tok[0], "avg") == 0) {
        return cmd_avg(tok, ntok);
    }
    if (strcmp(tok[0], "rate") == 0) {
        return cmd_rate(tok, ntok);
    }
    if (strcmp(tok[0], "path") == 0) {
        return cmd_path(tok, ntok);
    }
    if (strcmp(tok[0], "seg") == 0) {
        return cmd_seg(tok, ntok);
    }
    if (strcmp(tok[0], "seglen") == 0) {
        return cmd_seglen(tok, ntok);
    }
    if (strcmp(tok[0], "segno") == 0) {
        return cmd_segno(tok, ntok);
    }
    if (strcmp(tok[0], "segpage") == 0) {
        return cmd_segpage(tok, ntok);
    }
    if (strcmp(tok[0], "trace") == 0) {
        return cmd_trace(tok, ntok);
    }
    return fail("bad-command");
}
