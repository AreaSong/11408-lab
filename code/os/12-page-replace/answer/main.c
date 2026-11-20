#include "cmd.h"

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

int main(void) {
    char line[1024];

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
        if (cmd_line(tok, ntok) != 0) {
            return 1;
        }
    }
}
