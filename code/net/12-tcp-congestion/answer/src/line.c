#include "line.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

static int gap_pending;

int take_token(char **p, char *out, int cap) {
    char *s = *p;
    int n = 0;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }
    if (*s == '\0') {
        *p = s;
        return 0;
    }
    while (*s != '\0' && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
        if (n + 1 >= cap) {
            return -1;
        }
        out[n++] = *s++;
    }
    out[n] = '\0';
    *p = s;
    return 1;
}

int take_need(char **p, char *out, int cap, const char *what) {
    int rc = take_token(p, out, cap);

    if (rc == 0) {
        return reject(what);
    }
    if (rc < 0) {
        return reject("这一项太长");
    }
    return 0;
}

int rest_blank(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return *p == '\0';
}

int reject(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return -1;
}

void output_begin(void) {
    if (gap_pending) {
        fputc('\n', stdout);
    }
    gap_pending = 1;
}

static int blank_line(const char *line) {
    return rest_blank(line);
}

int command_loop(const char *hint, int (*run)(char *line)) {
    char line[1200];

    gap_pending = 0;
    while (1) {
        prompt("%s", hint);
        if (fgets(line, (int)sizeof line, stdin) == NULL) {
            break;
        }
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            return reject("这一行太长");
        }
        if (blank_line(line)) {
            continue;
        }
        if (run(line) != 0) {
            return -1;
        }
    }
    return 0;
}
