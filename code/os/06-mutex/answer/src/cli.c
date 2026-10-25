#include "cli.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

static void trim_newline(char *text) {
    size_t n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
        text[--n] = '\0';
    }
}

static int blank_line(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

bool cli_read(char *buf, size_t n, const char *prompt_text) {
    for (;;) {
        prompt("%s", prompt_text);
        if (fgets(buf, (int)n, stdin) == NULL) {
            return false;
        }
        trim_newline(buf);
        if (!blank_line(buf)) {
            return true;
        }
    }
}

int cli_split(char *line, char **tok, int max) {
    int n = 0;
    char *p = line;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (n >= max) {
            return -1;
        }
        tok[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return n;
}
