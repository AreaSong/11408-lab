#include "run.h"

#include <string.h>

static int skipped(const char *line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    return *line == '\0' || *line == '#';
}

static int is_calc(const char *name) {
    return strcmp(name, "phys") == 0 || strcmp(name, "inode") == 0 || strcmp(name, "bitmap") == 0 ||
           strcmp(name, "insert") == 0 || strcmp(name, "linkmax") == 0 || strcmp(name, "fat") == 0 ||
           strcmp(name, "group") == 0 || strcmp(name, "exam") == 0 || strcmp(name, "claim") == 0 ||
           strcmp(name, "card") == 0;
}

int main(void) {
    static Group group;
    static Image image;
    static MountSet mount;
    char line[512];

    group_init(&group, GROUP_CAP);
    image_init(&image);
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
        rc = is_calc(tok[0]) ? cmd_calc(&group, tok, ntok) : cmd_live(&image, &mount, tok, ntok);
        if (rc != 0) {
            return 1;
        }
    }
}
