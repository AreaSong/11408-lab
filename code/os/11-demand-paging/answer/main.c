#include "run.h"

#include <string.h>

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

static int route(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "vm") == 0) {
        return cmd_vm(tok, ntok);
    }
    if (strcmp(tok[0], "alloc") == 0) {
        return cmd_alloc(tok, ntok);
    }
    return cmd_rest(tok, ntok);
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
