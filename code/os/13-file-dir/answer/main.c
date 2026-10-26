#include "run.h"

#include <string.h>

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

static int route(Fs *fs, char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "asl") == 0 || strcmp(tok[0], "walk") == 0) {
        return cmd_lookup(tok, ntok);
    }
    if (strcmp(tok[0], "exam") == 0 || strcmp(tok[0], "claim") == 0 || strcmp(tok[0], "card") == 0) {
        return cmd_exam(tok, ntok);
    }
    return cmd_fs(fs, tok, ntok);
}

int main(void) {
    Fs fs;
    char line[512];

    memset(&fs, 0, sizeof fs);
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
        if (route(&fs, tok, ntok) != 0) {
            return 1;
        }
    }
}
