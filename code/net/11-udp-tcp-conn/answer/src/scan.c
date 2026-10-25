#include "scan.h"

#include "prompt.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

static int blank_line(const char *line) {
    return rest_blank(line);
}

int command_loop(const char *hint, int (*run)(char *line)) {
    char line[1200];
    int first = 1;

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
        if (!first) {
            fputc('\n', stdout);
        }
        first = 0;
        if (run(line) != 0) {
            return -1;
        }
    }
    return 0;
}

int parse_u32(const char *s, unsigned int *out) {
    char *end = NULL;
    unsigned long v;

    if (s[0] == '\0' || s[0] == '+' || s[0] == '-') {
        return 0;
    }
    errno = 0;
    v = strtoul(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v > 0xfffffffful) {
        return 0;
    }
    *out = (unsigned int)v;
    return 1;
}

int parse_ip(const char *s, unsigned char out[4]) {
    unsigned int a, b, c, d;
    char tail;

    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
        return 0;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return 0;
    }
    out[0] = (unsigned char)a;
    out[1] = (unsigned char)b;
    out[2] = (unsigned char)c;
    out[3] = (unsigned char)d;
    return 1;
}

int parse_port(const char *s, unsigned short *out) {
    unsigned int v;

    if (!parse_u32(s, &v) || v > 65535u) {
        return 0;
    }
    *out = (unsigned short)v;
    return 1;
}

static int hex_val(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

int parse_hex(const char *s, unsigned char *out, int cap, int *n) {
    int len = (int)strlen(s);
    int i;

    if (len == 0 || (len % 2) != 0 || len / 2 > cap) {
        return 0;
    }
    for (i = 0; i < len; i += 2) {
        int hi = hex_val(s[i]);
        int lo = hex_val(s[i + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i / 2] = (unsigned char)((hi << 4) | lo);
    }
    *n = len / 2;
    return 1;
}

int parse_bits16(const char *s, unsigned short *out) {
    unsigned int v = 0;
    int i;

    if (strlen(s) != 16) {
        return 0;
    }
    for (i = 0; i < 16; i++) {
        if (s[i] != '0' && s[i] != '1') {
            return 0;
        }
        v = (v << 1) | (unsigned int)(s[i] - '0');
    }
    *out = (unsigned short)v;
    return 1;
}
