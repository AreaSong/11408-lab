#include "loop.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

static int blank_line(const char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') {
        line++;
    }
    return *line == '\0';
}

int command_loop(const char *hint, int (*run)(const char *line)) {
    char line[1024];
    int first = 1;

    while (1) {
        prompt("%s", hint);
        if (fgets(line, (int)sizeof line, stdin) == NULL) {
            break;
        }
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "这一行太长\n");
            return 1;
        }
        if (blank_line(line)) {
            continue;
        }
        if (!first) {
            fputc('\n', stdout);
        }
        first = 0;
        if (run(line) != 0) {
            return 1;
        }
    }
    return 0;
}
