#include "cmd.h"

#include "exam.h"
#include "wset.h"

#include <string.h>

static int take_refs(char tok[][TOK_W], int from, int n, Access *refs, int *nr) {
    int i;

    *nr = 0;
    for (i = from; i < n; i++) {
        if (*nr >= REF_MAX || parse_ref(tok[i], &refs[*nr]) != 0) {
            return fail("bad-number");
        }
        (*nr)++;
    }
    if (*nr == 0) {
        return fail("bad-command");
    }
    return 0;
}

static int take_pages(char tok[][TOK_W], int from, int n, int *pages, int *np) {
    int i;

    *np = 0;
    for (i = from; i < n; i++) {
        if (*np >= REF_MAX || read_range(tok[i], 0, PAGE_MAX, &pages[*np]) != 0) {
            return fail("bad-number");
        }
        (*np)++;
    }
    if (*np == 0) {
        return fail("bad-command");
    }
    return 0;
}

static int report_of(int rc, const Report *r) {
    if (rc == -1) {
        return fail("bad-algo");
    }
    if (rc == -3) {
        return fail("duplicate");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    show_report(r);
    return 0;
}

static int cmd_run(char tok[][TOK_W], int n) {
    Access warm[FRAME_MAX];
    Access refs[REF_MAX];
    Report r;
    int frames = 0;
    int nw = 0;
    int nr = 0;
    int i = 3;

    if (n < 4 || read_range(tok[2], 1, FRAME_MAX, &frames) != 0) {
        return n < 4 ? fail("bad-command") : fail("bad-number");
    }
    if (strcmp(tok[3], "warm") == 0) {
        i = 4;
        while (i < n && strcmp(tok[i], "ref") != 0) {
            int page = 0;
            if (nw >= frames || read_range(tok[i], 0, PAGE_MAX, &page) != 0) {
                return fail("bad-number");
            }
            warm[nw].page = page;
            warm[nw].write = 0;
            nw++;
            i++;
        }
        if (nw == 0 || i >= n || strcmp(tok[i], "ref") != 0) {
            return fail("bad-command");
        }
        i++;
    }
    if (take_refs(tok, i, n, refs, &nr) != 0) {
        return 1;
    }
    return report_of(sim_exec(tok[1], frames, warm, nw, refs, nr, &r), &r);
}

static int cmd_preset(char tok[][TOK_W], int n) {
    int page[FRAME_MAX];
    int aref[FRAME_MAX];
    int dirty[FRAME_MAX];
    int ncell = 0;
    int i = 2;
    Access next;
    Report r;

    if (n < 4) {
        return fail("bad-command");
    }
    while (i < n && strcmp(tok[i], "ref") != 0) {
        if (ncell >= FRAME_MAX ||
            parse_cell(tok[i], &page[ncell], &aref[ncell], &dirty[ncell]) != 0) {
            return fail("bad-number");
        }
        ncell++;
        i++;
    }
    if (ncell == 0 || i + 2 != n || strcmp(tok[i], "ref") != 0) {
        return fail("bad-command");
    }
    if (parse_ref(tok[i + 1], &next) != 0) {
        return fail("bad-number");
    }
    return report_of(clock_preset(tok[1], page, aref, dirty, ncell, next, &r), &r);
}

static int pow2(int n) {
    return n >= 2 && (n & (n - 1)) == 0;
}

static int cmd_size(char tok[][TOK_W], int n) {
    int frames = 0;
    int bytes = 0;
    int addr[REF_MAX];
    int na = 0;
    int i;

    if (n < 5) {
        return fail("bad-command");
    }
    if (read_range(tok[2], 1, FRAME_MAX, &frames) != 0 || read_range(tok[3], 2, 1048576, &bytes) != 0 ||
        !pow2(bytes)) {
        return fail("bad-number");
    }
    for (i = 4; i < n; i++) {
        if (na >= REF_MAX || read_range(tok[i], 0, PAGE_MAX, &addr[na]) != 0) {
            return fail("bad-number");
        }
        if (addr[na] / bytes > PAGE_MAX) {
            return fail("bad-number");
        }
        na++;
    }
    if (na == 0 || algo_id(tok[1]) < 0) {
        return na == 0 ? fail("bad-command") : fail("bad-algo");
    }
    show_size(tok[1], frames, bytes, addr, na);
    return 0;
}

static int cmd_sweep(char tok[][TOK_W], int n) {
    Access refs[REF_MAX];
    int nr = 0;
    int frames = 0;

    if (n < 4) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "frames") == 0) {
        if (algo_id(tok[2]) < 0) {
            return fail("bad-algo");
        }
        if (take_refs(tok, 3, n, refs, &nr) != 0) {
            return 1;
        }
        show_sweep_frames(tok[2], refs, nr);
        return 0;
    }
    if (strcmp(tok[1], "algos") == 0) {
        if (read_range(tok[2], 1, FRAME_MAX, &frames) != 0) {
            return fail("bad-number");
        }
        if (take_refs(tok, 3, n, refs, &nr) != 0) {
            return 1;
        }
        show_sweep_algos(frames, refs, nr);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_ws(char tok[][TOK_W], int n) {
    int window = 0;
    int pages[REF_MAX];
    int np = 0;

    if (n < 3 || read_range(tok[1], 1, REF_MAX, &window) != 0) {
        return n < 3 ? fail("bad-command") : fail("bad-number");
    }
    if (take_pages(tok, 2, n, pages, &np) != 0) {
        return 1;
    }
    show_work(window, pages, np);
    return 0;
}

static int cmd_thrash(char tok[][TOK_W], int n) {
    Access refs[REF_MAX];
    int nr = 0;

    if (n < 3) {
        return fail("bad-command");
    }
    if (algo_id(tok[1]) < 0) {
        return fail("bad-algo");
    }
    if (take_refs(tok, 2, n, refs, &nr) != 0) {
        return 1;
    }
    show_thrash(tok[1], refs, nr);
    return 0;
}

static int cmd_rest(char tok[][TOK_W], int n) {
    int frames = 0;

    if (strcmp(tok[0], "locality") == 0) {
        if (n != 2 || read_range(tok[1], 1, FRAME_MAX, &frames) != 0) {
            return n != 2 ? fail("bad-command") : fail("bad-number");
        }
        show_locality(frames);
        return 0;
    }
    if (strcmp(tok[0], "belady") == 0) {
        if (n != 1) {
            return fail("bad-command");
        }
        show_belady();
        return 0;
    }
    if (strcmp(tok[0], "exam") == 0) {
        return n == 2 ? exam_print(tok[1]) : fail("bad-command");
    }
    if (strcmp(tok[0], "claim") == 0) {
        return n == 2 ? claim_print(tok[1]) : fail("bad-command");
    }
    if (strcmp(tok[0], "card") == 0) {
        if (n != 1) {
            return fail("bad-command");
        }
        card_print();
        return 0;
    }
    return fail("bad-command");
}

int cmd_line(char tok[][TOK_W], int n) {
    if (n < 1) {
        return fail("bad-command");
    }
    if (strcmp(tok[0], "run") == 0) {
        return cmd_run(tok, n);
    }
    if (strcmp(tok[0], "preset") == 0) {
        return cmd_preset(tok, n);
    }
    if (strcmp(tok[0], "ws") == 0) {
        return cmd_ws(tok, n);
    }
    if (strcmp(tok[0], "thrash") == 0) {
        return cmd_thrash(tok, n);
    }
    if (strcmp(tok[0], "size") == 0) {
        return cmd_size(tok, n);
    }
    if (strcmp(tok[0], "sweep") == 0) {
        return cmd_sweep(tok, n);
    }
    return cmd_rest(tok, n);
}
