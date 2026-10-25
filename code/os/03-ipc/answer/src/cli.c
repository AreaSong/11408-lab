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
