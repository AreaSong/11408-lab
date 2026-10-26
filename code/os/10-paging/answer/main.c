#include "cmd.h"
#include "exam.h"

#include <stdio.h>
#include <string.h>

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

static int known(const char *name) {
    return strcmp(name, "geom") == 0 || strcmp(name, "cut") == 0 || strcmp(name, "pack") == 0 ||
           strcmp(name, "slot") == 0 || strcmp(name, "join") == 0 || strcmp(name, "span") == 0 ||
           strcmp(name, "elem") == 0 || strcmp(name, "dirs") == 0 || strcmp(name, "frag") == 0 ||
           strcmp(name, "share") == 0;
}

static int route(char tok[][TOK_W], int ntok) {
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
    if (known(tok[0])) {
        return cmd_calc(tok, ntok);
    }
    return cmd_walk(tok, ntok);
}

int main(void) {
    char line[512];

    for (;;) {
        char tok[TOK_N][TOK_W];
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
        if (route(tok, ntok) != 0) {
            return 1;
        }
    }
}
