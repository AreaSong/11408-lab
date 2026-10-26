#include "run.h"

#include "asl.h"
#include "exam.h"

#include <stdio.h>
#include <string.h>

static int print_asl(const char *org, int n, int groups, int binary) {
    Frac frac;
    char sizes[128];
    int rc;
    int shaped = 0;

    if (binary < 0) {
        rc = strcmp(org, "seq") == 0 ? asl_seq(n, &frac) : asl_index(n, &frac);
        groups = 0;
    } else {
        rc = asl_isam(n, groups, binary, &frac);
        if (rc == 0) {
            shaped = asl_sizes(n, groups, sizes, (int)sizeof sizes);
        }
    }
    if (rc != 0 || shaped < 0) {
        return fail("bad-number");
    }
    if (groups == 0) {
        printf("%s n=%d cmp=%lld/%lld\n", org, n, frac.num, frac.den);
    } else if (shaped == 1) {
        printf("%s n=%d groups=%d size=%s cmp=%lld/%lld\n", org, n, groups, sizes, frac.num,
               frac.den);
    } else {
        printf("%s n=%d groups=%d cmp=%lld/%lld\n", org, n, groups, frac.num, frac.den);
    }
    return 0;
}

static int cmd_all(int n, int groups) {
    Frac probe;

    if (asl_seq(n, &probe) != 0 || asl_isam(n, groups, 0, &probe) != 0) {
        return fail("bad-number");
    }
    if (print_asl("seq", n, 0, -1) != 0 || print_asl("index", n, 0, -1) != 0) {
        return 1;
    }
    if (print_asl("isam-seq", n, groups, 0) != 0) {
        return 1;
    }
    return print_asl("isam-bin", n, groups, 1);
}

static int org_of(const char *name) {
    if (strcmp(name, "seq") == 0) {
        return ASL_SEQ;
    }
    if (strcmp(name, "index") == 0) {
        return ASL_INDEX;
    }
    if (strcmp(name, "isam-seq") == 0) {
        return ASL_ISAM_SEQ;
    }
    if (strcmp(name, "isam-bin") == 0) {
        return ASL_ISAM_BIN;
    }
    return -1;
}

static int cmd_walk(int org, int n, int groups, int key) {
    AslWalk walk;
    int rc = asl_walk(org, n, groups, key, &walk);
    int i;
    const char *name = "seq";

    if (rc == -2) {
        return fail("bad-number");
    }
    if (rc != 0) {
        return fail("bad-number");
    }
    if (org == ASL_INDEX) {
        name = "index";
    } else if (org == ASL_ISAM_SEQ) {
        name = "isam-seq";
    } else if (org == ASL_ISAM_BIN) {
        name = "isam-bin";
    }
    for (i = 0; i < walk.nstep; i++) {
        printf("%s\n", walk.step[i]);
    }
    if (org == ASL_SEQ || org == ASL_INDEX) {
        printf("walk org=%s n=%d key=%d cmp=%d\n", name, n, key, walk.cmp);
    } else {
        printf("walk org=%s n=%d groups=%d key=%d cmp=%d\n", name, n, groups, key, walk.cmp);
    }
    return 0;
}

static int asl_cmd(char tok[][TOK_W], int ntok) {
    int n = 0;
    int groups = 0;

    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "all") == 0) {
        if (need(ntok, 4) != 0) {
            return 1;
        }
        if (read_i(tok[2], &n) != 0 || read_i(tok[3], &groups) != 0) {
            return fail("bad-number");
        }
        return cmd_all(n, groups);
    }
    if (strcmp(tok[1], "seq") == 0 || strcmp(tok[1], "index") == 0) {
        if (need(ntok, 3) != 0) {
            return 1;
        }
        if (read_i(tok[2], &n) != 0) {
            return fail("bad-number");
        }
        return print_asl(tok[1], n, 0, -1);
    }
    if (strcmp(tok[1], "isam") == 0) {
        int binary;
        if (need(ntok, 5) != 0) {
            return 1;
        }
        if (read_i(tok[2], &n) != 0 || read_i(tok[3], &groups) != 0) {
            return fail("bad-number");
        }
        if (strcmp(tok[4], "seq") == 0) {
            binary = 0;
        } else if (strcmp(tok[4], "bin") == 0) {
            binary = 1;
        } else {
            return fail("bad-command");
        }
        return print_asl(binary ? "isam-bin" : "isam-seq", n, groups, binary);
    }
    return fail("bad-command");
}

static int walk_cmd(char tok[][TOK_W], int ntok) {
    int org;
    int n = 0;
    int groups = 0;
    int key = 0;

    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[1], "isam") == 0) {
        if (need(ntok, 6) != 0) {
            return 1;
        }
        if (read_i(tok[2], &n) != 0 || read_i(tok[3], &groups) != 0 || read_i(tok[5], &key) != 0) {
            return fail("bad-number");
        }
        if (strcmp(tok[4], "seq") == 0) {
            org = ASL_ISAM_SEQ;
        } else if (strcmp(tok[4], "bin") == 0) {
            org = ASL_ISAM_BIN;
        } else {
            return fail("bad-command");
        }
        return cmd_walk(org, n, groups, key);
    }
    org = org_of(tok[1]);
    if (org != ASL_SEQ && org != ASL_INDEX) {
        return fail("bad-command");
    }
    if (need(ntok, 4) != 0) {
        return 1;
    }
    if (read_i(tok[2], &n) != 0 || read_i(tok[3], &key) != 0) {
        return fail("bad-number");
    }
    return cmd_walk(org, n, 0, key);
}

int cmd_lookup(char tok[][TOK_W], int ntok) {
    if (ntok < 2) {
        return fail("bad-command");
    }
    if (strcmp(tok[0], "asl") == 0) {
        return asl_cmd(tok, ntok);
    }
    if (strcmp(tok[0], "walk") == 0) {
        return walk_cmd(tok, ntok);
    }
    return fail("bad-command");
}

int cmd_exam(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "card") == 0) {
        if (need(ntok, 1) != 0) {
            return 1;
        }
        card_print();
        return 0;
    }
    if (need(ntok, 2) != 0) {
        return 1;
    }
    if (strcmp(tok[0], "exam") == 0) {
        if (exam_print(tok[1]) != 0) {
            return fail("bad-exam");
        }
        return 0;
    }
    if (strcmp(tok[0], "claim") == 0) {
        if (claim_print(tok[1]) != 0) {
            return fail("bad-claim");
        }
        return 0;
    }
    return fail("bad-command");
}
