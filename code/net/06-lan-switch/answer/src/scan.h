#ifndef SCAN_H
#define SCAN_H

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prompt.h"

#define LAB_ERR 160

static inline int scan_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline const char *scan_skip(const char *s) {
    while (scan_space(*s)) {
        s++;
    }
    return s;
}

static inline int scan_blank(const char *s) {
    return *scan_skip(s) == '\0';
}

/* 1 读到一个词，0 没有了，-1 这个词太长。 */
static inline int scan_word(const char **ps, char *buf, int cap) {
    const char *s = scan_skip(*ps);
    int n = 0;

    if (*s == '\0') {
        *ps = s;
        return 0;
    }
    while (*s != '\0' && !scan_space(*s)) {
        if (n + 1 >= cap) {
            return -1;
        }
        buf[n++] = *s++;
    }
    buf[n] = '\0';
    *ps = s;
    return 1;
}

static inline int scan_int(const char *s, int *out) {
    char *end = NULL;
    long v;

    if (s == NULL || s[0] == '\0' || isdigit((unsigned char)s[0]) == 0) {
        return 0;
    }
    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno != 0 || v > 1000000L) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

/* 接受 0800 或 0x0800，范围是 0～0xffff。 */
static inline int scan_hex16(const char *s, unsigned *out) {
    char *end = NULL;
    unsigned long v;

    if (s == NULL) {
        return 0;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    if (s[0] == '\0' || isxdigit((unsigned char)s[0]) == 0) {
        return 0;
    }
    errno = 0;
    v = strtoul(s, &end, 16);
    if (end == s || *end != '\0' || errno != 0 || v > 0xffffUL) {
        return 0;
    }
    *out = (unsigned)v;
    return 1;
}

static inline int lab_fail(char *err, const char *msg) {
    if (err != NULL) {
        snprintf(err, LAB_ERR, "%s", msg);
    }
    return 0;
}

static inline int lab_lines(const char *hint, int (*run)(const char *line)) {
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
        if (scan_blank(line)) {
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

#endif
