#include "scan.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

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

int take_int(char **p, int *out) {
    char tok[16];
    int rc = take_token(p, tok, (int)sizeof tok);
    int i = 0;
    int v = 0;

    if (rc != 1) {
        return rc;
    }
    if (tok[0] < '0' || tok[0] > '9') {
        return -1;
    }
    while (tok[i] != '\0') {
        int digit;
        if (tok[i] < '0' || tok[i] > '9') {
            return -1;
        }
        digit = tok[i] - '0';
        if (v > 1000000 / 10) {
            return -1;
        }
        v = v * 10 + digit;
        i++;
    }
    *out = v;
    return 1;
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

static int blank_line(const char *line) {
    return rest_blank(line);
}

int command_loop(const char *hint, int (*run)(char *line)) {
    char line[1024];

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
