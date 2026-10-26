#include "run.h"

#include "buf.h"
#include "fact.h"

#include <stdio.h>
#include <string.h>

static int named(const char *text, int hi) {
    if (token_name(text, 1, hi) != 0) {
        return fail("bad-command");
    }
    return 0;
}

static int cmd_buf(char tok[][TOK_W], int ntok) {
    int trace = 0;
    int base = 1;
    int kind;
    int n;
    int t;
    int m;
    int c;

    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "trace") == 0) {
        trace = 1;
        base = 2;
    }
    if (need(ntok, base + 5) != 0) {
        return 1;
    }
    if (strcmp(tok[base], "single") == 0) {
        kind = 0;
    } else if (strcmp(tok[base], "double") == 0) {
        kind = 1;
    } else if (strcmp(tok[base], "both") == 0) {
        kind = 2;
    } else {
        return fail("bad-command");
    }
    if (take_i(tok[base + 1], 1, trace ? 16 : 10000, &n) != 0) {
        return 1;
    }
    if (take_i(tok[base + 2], 0, 1000000, &t) != 0 || take_i(tok[base + 3], 0, 1000000, &m) != 0 ||
        take_i(tok[base + 4], 0, 1000000, &c) != 0) {
        return 1;
    }
    if (buf_report(kind, trace, n, t, m, c) != 0) {
        return fail("bad-sim");
    }
    return 0;
}

static int cmd_ring(Ring *r, char tok[][TOK_W], int ntok) {
    int cap;

    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "new") == 0) {
        if (need(ntok, 3) != 0 || take_i(tok[2], 1, RING_MAX, &cap) != 0) {
            return 1;
        }
        return ring_new(r, cap);
    }
    if (!r->ready) {
        return fail("no-ring");
    }
    if (strcmp(tok[1], "put") == 0) {
        if (need(ntok, 3) != 0 || named(tok[2], 12) != 0) {
            return 1;
        }
        return ring_put(r, tok[2]);
    }
    if (strcmp(tok[1], "get") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        return ring_get(r);
    }
    if (strcmp(tok[1], "show") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        ring_show(r);
        return 0;
    }
    return fail("bad-command");
}

static int dev_add(IoSys *s, char tok[][TOK_W], int ntok) {
    if (need(ntok, 6) != 0) {
        return 1;
    }
    if (strcmp(tok[2], "block") != 0 && strcmp(tok[2], "char") != 0) {
        return fail("bad-command");
    }
    if (named(tok[3], 8) != 0 || named(tok[4], 8) != 0 || named(tok[5], 8) != 0) {
        return 1;
    }
    return io_add(s, tok[2], tok[3], tok[4], tok[5]);
}

static int dev_proc(IoSys *s, char tok[][TOK_W], int ntok) {
    if (need(ntok, 3) != 0 || named(tok[2], 8) != 0) {
        return 1;
    }
    return io_proc(s, tok[2]);
}

static int dev_map(IoSys *s, char tok[][TOK_W], int ntok) {
    if (need(ntok, 5) != 0 || named(tok[2], 8) != 0 || named(tok[3], 8) != 0 ||
        named(tok[4], 8) != 0) {
        return 1;
    }
    return io_map(s, tok[2], tok[3], tok[4]);
}

static int dev_ask(IoSys *s, char tok[][TOK_W], int ntok) {
    if (need(ntok, 4) != 0 || named(tok[2], 8) != 0 || named(tok[3], 8) != 0) {
        return 1;
    }
    return io_ask(s, tok[2], tok[3]);
}

static int dev_done(IoSys *s, char tok[][TOK_W], int ntok) {
    if (need(ntok, 3) != 0 || named(tok[2], 8) != 0) {
        return 1;
    }
    return io_done(s, tok[2]);
}

static int cmd_dev(IoSys *s, char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "reset") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        io_init(s);
        printf("dev reset\n");
        return 0;
    }
    if (strcmp(tok[1], "add") == 0) {
        return dev_add(s, tok, ntok);
    }
    if (strcmp(tok[1], "proc") == 0) {
        return dev_proc(s, tok, ntok);
    }
    if (strcmp(tok[1], "map") == 0) {
        return dev_map(s, tok, ntok);
    }
    if (strcmp(tok[1], "ask") == 0) {
        return dev_ask(s, tok, ntok);
    }
    if (strcmp(tok[1], "done") == 0) {
        return dev_done(s, tok, ntok);
    }
    if (strcmp(tok[1], "show") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        io_show(s);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_spool(Spool *s, char tok[][TOK_W], int ntok) {
    int cap;

    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "parts") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        spool_parts();
        return 0;
    }
    if (strcmp(tok[1], "reset") == 0) {
        if (need(ntok, 3) != 0 || take_i(tok[2], 1, SPOOL_MAX, &cap) != 0) {
            return 1;
        }
        spool_init(s, cap);
        return 0;
    }
    if (!s->ready) {
        return fail("no-spool");
    }
    if (strcmp(tok[1], "submit") == 0) {
        if (need(ntok, 4) != 0 || named(tok[2], 8) != 0 || named(tok[3], 12) != 0) {
            return 1;
        }
        return spool_submit(s, tok[2], tok[3]);
    }
    if (strcmp(tok[1], "print") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        return spool_print(s);
    }
    if (strcmp(tok[1], "finish") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        return spool_finish(s);
    }
    if (strcmp(tok[1], "show") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        spool_show(s);
        return 0;
    }
    return fail("bad-command");
}

static int cmd_io(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "layer") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        layer_print();
        return 0;
    }
    if (strcmp(tok[1], "ctrl") == 0) {
        if (need(ntok, 3) != 0) {
            return 1;
        }
        return ctrl_print(tok[2]) == 0 ? 0 : fail("bad-command");
    }
    if (strcmp(tok[1], "wait") == 0) {
        if (need(ntok, 3) != 0) {
            return 1;
        }
        return wait_print(tok[2]) == 0 ? 0 : fail("bad-command");
    }
    return fail("bad-command");
}

static int cmd_exam(char tok[][TOK_W], int ntok) {
    int rc;

    if (need(ntok, 2) != 0) {
        return 1;
    }
    rc = exam_print(tok[1]);
    if (rc == -2) {
        return fail("bad-sim");
    }
    if (rc != 0) {
        return fail("bad-exam");
    }
    return 0;
}

static int cmd_claim(char tok[][TOK_W], int ntok) {
    if (need(ntok, 2) != 0) {
        return 1;
    }
    if (claim_print(tok[1]) != 0) {
        return fail("bad-claim");
    }
    return 0;
}

int cmd_dispatch(IoSys *io, Spool *sp, Ring *rg, char tok[][TOK_W], int ntok) {
    if (ntok < 1) {
        return fail("bad-command");
    }
    if (strcmp(tok[0], "buf") == 0) {
        return cmd_buf(tok, ntok);
    }
    if (strcmp(tok[0], "ring") == 0) {
        return cmd_ring(rg, tok, ntok);
    }
    if (strcmp(tok[0], "dev") == 0) {
        return cmd_dev(io, tok, ntok);
    }
    if (strcmp(tok[0], "spool") == 0) {
        return cmd_spool(sp, tok, ntok);
    }
    if (strcmp(tok[0], "io") == 0) {
        return cmd_io(tok, ntok);
    }
    if (strcmp(tok[0], "exam") == 0) {
        return cmd_exam(tok, ntok);
    }
    if (strcmp(tok[0], "claim") == 0) {
        return cmd_claim(tok, ntok);
    }
    if (strcmp(tok[0], "card") == 0) {
        if (need(ntok, 1) != 0) {
            return 1;
        }
        card_print();
        return 0;
    }
    return fail("bad-command");
}
