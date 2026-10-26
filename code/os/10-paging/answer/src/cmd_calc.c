#include "cmd.h"

#include "addr.h"
#include "geom.h"

#include <stdio.h>
#include <string.h>

static int read_width(const char *text, int *out) {
    unsigned long long value;

    if (read_ull(text, &value) != 0 || value < 1 || value > 63) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static void print_geom(const Geom *g, const char *mode) {
    int i;

    printf("geom mode=%s va=%d page=%llu pte=%llu\n", mode, g->va_bits, g->page_bytes, g->pte_bytes);
    printf("offset-bits=%d page-bits=%d pages=%llu ", g->offset_bits, g->page_bits, g->pages);
    if (g->table_overflow) {
        printf("table-bytes=overflow\n");
    } else {
        printf("table-bytes=%llu\n", g->table_bytes);
    }
    printf("per-page=%llu level-bits=%d levels=%d\n", g->per_page, g->level_bits, g->levels);
    printf("top-bits=%d top-entries=%llu top-bytes=%llu fits=%s\n", g->top_bits, g->top_entries,
           g->top_bytes, g->fits ? "yes" : "no");
    printf("tables=");
    if (g->nterms == 0) {
        printf("0");
    }
    for (i = 0; i < g->nterms; i++) {
        if (i > 0) {
            printf("+");
        }
        printf("%llu", g->terms[i]);
    }
    if (g->terms_overflow) {
        printf("+overflow pages-used=overflow\n");
    } else {
        printf(" pages-used=%llu\n", g->tables_pages);
    }
}

static int cmd_geom(char tok[][TOK_W], int ntok) {
    unsigned long long args[3];
    Geom geom;

    if (need_args(ntok, 5) || read_args(tok, 2, 3, args)) {
        return 1;
    }
    if (strcmp(tok[1], "bits") == 0) {
        if (args[0] < 1 || args[0] > 64) {
            return fail("bad-number");
        }
        if (geom_bits((int)args[0], args[1], args[2], &geom) != 0) {
            return fail("bad-size");
        }
    } else if (strcmp(tok[1], "pages") == 0) {
        if (geom_pages(args[0], args[1], args[2], &geom) != 0) {
            return fail("bad-size");
        }
    } else {
        return fail("bad-command");
    }
    print_geom(&geom, tok[1]);
    return 0;
}

static int cmd_cut(char tok[][TOK_W], int ntok) {
    int n = ntok - 2;
    int widths[4];
    unsigned long long parts[4];
    unsigned long long addr;
    int i;

    if (n < 2 || n > 4) {
        return fail("bad-command");
    }
    for (i = 0; i < n; i++) {
        if (read_width(tok[1 + i], &widths[i]) != 0) {
            return fail("bad-number");
        }
    }
    if (read_ull(tok[ntok - 1], &addr) != 0) {
        return fail("bad-number");
    }
    if (addr_cut(widths, n, addr, parts) != 0) {
        return fail("bad-number");
    }
    printf("cut addr=");
    print_hex(addr);
    printf(" parts=");
    for (i = 0; i < n; i++) {
        if (i > 0) {
            printf(",");
        }
        print_hex(parts[i]);
    }
    printf("\n");
    return 0;
}

static int cmd_pack(char tok[][TOK_W], int ntok) {
    int n;
    int widths[4];
    unsigned long long values[4];
    unsigned long long addr;
    int i;

    if (ntok < 5 || (ntok - 1) % 2 != 0) {
        return fail("bad-command");
    }
    n = (ntok - 1) / 2;
    if (n < 2 || n > 4) {
        return fail("bad-command");
    }
    for (i = 0; i < n; i++) {
        if (read_width(tok[1 + i], &widths[i]) != 0 || read_ull(tok[1 + n + i], &values[i]) != 0) {
            return fail("bad-number");
        }
    }
    if (addr_pack(widths, values, n, &addr) != 0) {
        return fail("bad-number");
    }
    printf("pack addr=");
    print_hex(addr);
    printf(" bits=%d\n", widths[0] + widths[1] + (n > 2 ? widths[2] : 0) + (n > 3 ? widths[3] : 0));
    return 0;
}

static int cmd_slot(char tok[][TOK_W], int ntok) {
    unsigned long long args[3];
    unsigned long long addr;

    if (need_args(ntok, 4) || read_args(tok, 1, 3, args)) {
        return 1;
    }
    if (addr_slot(args[0], args[1], args[2], &addr) != 0) {
        return fail("bad-number");
    }
    printf("slot index=%llu addr=", args[2]);
    print_hex(addr);
    printf("\n");
    return 0;
}

static int cmd_join(char tok[][TOK_W], int ntok) {
    unsigned long long args[3];
    unsigned long long pa;

    if (need_args(ntok, 4) || read_args(tok, 1, 3, args)) {
        return 1;
    }
    if (addr_join(args[0], args[1], args[2], &pa) != 0) {
        return fail("bad-number");
    }
    printf("join frame=");
    print_hex(args[1]);
    printf(" offset=");
    print_hex(args[2]);
    printf(" pa=");
    print_hex(pa);
    printf("\n");
    return 0;
}

static int cmd_span(char tok[][TOK_W], int ntok) {
    unsigned long long args[6];
    int count = 0;
    int i;

    if (need_args(ntok, 7) || read_args(tok, 1, 6, args)) {
        return 1;
    }
    if (span_count(args[0], args[3], &count) != 0) {
        return fail("bad-size");
    }
    printf("span pages=%d\n", count);
    for (i = 0; i < count; i++) {
        unsigned long long page = 0;
        unsigned long long pte = 0;
        unsigned long long frame = 0;
        unsigned long long pa = 0;
        if (span_at(args[0], args[1], args[2], args[4], args[5], i, &page, &pte, &frame, &pa) != 0) {
            return fail("bad-size");
        }
        printf("which=%d page=%llu pte=", i + 1, page);
        print_hex(pte);
        printf(" frame=");
        print_hex(frame);
        printf(" pa=");
        print_hex(pa);
        printf("\n");
    }
    return 0;
}

static int cmd_elem(char tok[][TOK_W], int ntok) {
    unsigned long long args[5];
    unsigned long long index = 0;
    unsigned long long addr = 0;

    if (need_args(ntok, 6) || read_args(tok, 1, 5, args)) {
        return 1;
    }
    if (addr_elem(args[0], args[1], args[2], args[3], args[4], &index, &addr) != 0) {
        return fail("bad-number");
    }
    printf("elem index=%llu addr=", index);
    print_hex(addr);
    printf("\n");
    return 0;
}

static int one_dir(int dir_bits, int page_bits, int off_bits, unsigned long long addr,
                   unsigned long long *dir) {
    int widths[3];
    unsigned long long parts[3];

    widths[0] = dir_bits;
    widths[1] = page_bits;
    widths[2] = off_bits;
    if (addr_cut(widths, 3, addr, parts) != 0) {
        return -1;
    }
    *dir = parts[0];
    return 0;
}

static int cmd_dirs(char tok[][TOK_W], int ntok) {
    unsigned long long args[5];
    unsigned long long dir_a = 0;
    unsigned long long dir_b = 0;
    int bits[3];
    int i;

    if (need_args(ntok, 6) || read_args(tok, 1, 5, args)) {
        return 1;
    }
    for (i = 0; i < 3; i++) {
        if (args[i] < 1 || args[i] > 63) {
            return fail("bad-number");
        }
        bits[i] = (int)args[i];
    }
    if (one_dir(bits[0], bits[1], bits[2], args[3], &dir_a) != 0 ||
        one_dir(bits[0], bits[1], bits[2], args[4], &dir_b) != 0) {
        return fail("bad-number");
    }
    printf("dirs a=");
    print_hex(args[3]);
    printf(" dir=");
    print_hex(dir_a);
    printf(" b=");
    print_hex(args[4]);
    printf(" dir=");
    print_hex(dir_b);
    printf(" tables=%d\n", dir_a == dir_b ? 1 : 2);
    return 0;
}

static int cmd_frag(char tok[][TOK_W], int ntok) {
    unsigned long long args[2];
    unsigned long long pages = 0;
    unsigned long long internal = 0;

    if (need_args(ntok, 3) || read_args(tok, 1, 2, args)) {
        return 1;
    }
    if (addr_frag(args[0], args[1], &pages, &internal) != 0) {
        return fail("bad-size");
    }
    printf("frag page=%llu bytes=%llu pages=%llu internal=%llu\n", args[0], args[1], pages, internal);
    return 0;
}

static int cmd_share(char tok[][TOK_W], int ntok) {
    unsigned long long args[5];
    unsigned long long pa = 0;

    if (need_args(ntok, 6) || read_args(tok, 1, 5, args)) {
        return 1;
    }
    if (addr_join(args[0], args[3], args[4], &pa) != 0) {
        return fail("bad-number");
    }
    printf("share page-a=%llu page-b=%llu frame=", args[1], args[2]);
    print_hex(args[3]);
    printf(" pa=");
    print_hex(pa);
    printf(" same=yes\n");
    return 0;
}

int cmd_calc(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "geom") == 0) {
        return cmd_geom(tok, ntok);
    }
    if (strcmp(tok[0], "cut") == 0) {
        return cmd_cut(tok, ntok);
    }
    if (strcmp(tok[0], "pack") == 0) {
        return cmd_pack(tok, ntok);
    }
    if (strcmp(tok[0], "slot") == 0) {
        return cmd_slot(tok, ntok);
    }
    if (strcmp(tok[0], "join") == 0) {
        return cmd_join(tok, ntok);
    }
    if (strcmp(tok[0], "span") == 0) {
        return cmd_span(tok, ntok);
    }
    if (strcmp(tok[0], "elem") == 0) {
        return cmd_elem(tok, ntok);
    }
    if (strcmp(tok[0], "dirs") == 0) {
        return cmd_dirs(tok, ntok);
    }
    if (strcmp(tok[0], "frag") == 0) {
        return cmd_frag(tok, ntok);
    }
    if (strcmp(tok[0], "share") == 0) {
        return cmd_share(tok, ntok);
    }
    return fail("bad-command");
}
