#include "http.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void trim_end(char *s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static int next_line(const char **p, char *out, int cap) {
    const char *s = *p;
    int n = 0;

    if (*s == '\0') {
        return 1;
    }
    while (*s != '\0' && *s != '\n') {
        if (n + 1 >= cap) {
            return -1;
        }
        out[n++] = *s++;
    }
    if (*s == '\n') {
        s++;
    }
    out[n] = '\0';
    trim_end(out);
    *p = s;
    return 0;
}

static int eq_ci(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int key_is(const char *line, const char *key, const char **val) {
    size_t n = strlen(key);
    size_t i;

    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)line[i]) != tolower((unsigned char)key[i])) {
            return 0;
        }
    }
    if (line[n] != ':') {
        return 0;
    }
    *val = line + n + 1;
    while (**val == ' ' || **val == '\t') {
        (*val)++;
    }
    return 1;
}

static int copy_val(char *dst, int cap, const char *src) {
    size_t n = strlen(src);

    if (n == 0 || n >= (size_t)cap) {
        return -1;
    }
    memcpy(dst, src, n + 1);
    return 0;
}

static int parse_request_line(const char *line, HttpReq *out) {
    char extra[8];

    if (sscanf(line, "%15s %255s %15s %7s", out->method, out->path, out->version, extra) != 3) {
        return -1;
    }
    if (strcmp(out->version, "HTTP/1.0") != 0 && strcmp(out->version, "HTTP/1.1") != 0) {
        return -1;
    }
    return 0;
}

static void apply_conn(HttpReq *out, const char *val) {
    /* close 是非持续。HTTP/1.1 默认持续，只有显式 close 才拆连接。 */
    if (eq_ci(val, "close")) {
        out->persistent = 0;
    } else if (eq_ci(val, "keep-alive")) {
        out->persistent = 1;
    }
}

static int apply_header(const char *line, HttpReq *out) {
    const char *val = NULL;

    if (key_is(line, "Host", &val)) {
        return copy_val(out->host, (int)sizeof out->host, val);
    }
    if (key_is(line, "Cookie", &val)) {
        out->has_cookie = 1;
        return copy_val(out->cookie, (int)sizeof out->cookie, val);
    }
    if (key_is(line, "Connection", &val)) {
        apply_conn(out, val);
    }
    return 0;
}

static int cookie_visit(const char *cookie, int *seen) {
    const char *p = cookie;

    *seen = 0;
    while ((p = strstr(p, "visit=")) != NULL) {
        int boundary = (p == cookie) || p[-1] == ' ' || p[-1] == ';';
        const char *d = p + 6;
        int v = 0;

        if (!boundary || *d < '0' || *d > '9') {
            p += 6;
            continue;
        }
        while (*d >= '0' && *d <= '9') {
            v = v * 10 + (*d - '0');
            if (v > 1000000) {
                return -1;
            }
            d++;
        }
        *seen = 1;
        return v;
    }
    return 0;
}

static int is_page(const char *path) {
    return strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0;
}

static void fill_visit(HttpReq *out) {
    int seen = 0;
    int v;

    if (!is_page(out->path)) {
        out->visit = 0;
        return;
    }
    v = cookie_visit(out->cookie, &seen);
    out->visit = (seen && v >= 0) ? v + 1 : 1;
}

int http_parse(const char *raw, HttpReq *out) {
    char line[512];
    const char *p = raw;
    int rc;

    memset(out, 0, sizeof *out);
    rc = next_line(&p, line, (int)sizeof line);
    if (rc != 0 || parse_request_line(line, out) != 0) {
        return -1;
    }
    /* HTTP/1.0 默认非持续，1.1 默认持续。Connection 可以改这个默认。 */
    out->persistent = strcmp(out->version, "HTTP/1.1") == 0;
    for (;;) {
        rc = next_line(&p, line, (int)sizeof line);
        if (rc == 1) {
            break;
        }
        if (rc != 0) {
            return -1;
        }
        if (line[0] == '\0') {
            break;
        }
        if (apply_header(line, out) != 0) {
            return -1;
        }
    }
    fill_visit(out);
    return 0;
}

static int body_of(const HttpReq *req, char *body, int cap) {
    if (strcmp(req->path, "/a.txt") == 0) {
        snprintf(body, (size_t)cap, "object-a\n");
        return 0;
    }
    if (strcmp(req->path, "/b.txt") == 0) {
        snprintf(body, (size_t)cap, "object-b\n");
        return 0;
    }
    if (!is_page(req->path)) {
        return -1;
    }
    snprintf(body, (size_t)cap,
             "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>visit</title></head>"
             "<body><p>第 %d 次访问</p>"
             "<p><a href=\"/a.txt\">a.txt</a> <a href=\"/b.txt\">b.txt</a></p>"
             "</body></html>\n",
             req->visit);
    return 1;
}

static const char *status_of(const HttpReq *req, char *body, int cap, const char **type, int *page) {
    int kind;

    *page = 0;
    *type = "text/plain; charset=utf-8";
    if (strcmp(req->method, "GET") != 0) {
        snprintf(body, (size_t)cap, "只接受 GET\n");
        return "405 Method Not Allowed";
    }
    kind = body_of(req, body, cap);
    if (kind < 0) {
        snprintf(body, (size_t)cap, "没有这个路径\n");
        return "404 Not Found";
    }
    if (kind == 1) {
        *page = 1;
        *type = "text/html; charset=utf-8";
    }
    return "200 OK";
}

int http_reply(const HttpReq *req, char *buf, int cap) {
    char body[1024];
    char extra[64];
    const char *type = NULL;
    const char *ver = "HTTP/1.1";
    int page = 0;
    int n;
    const char *status = status_of(req, body, (int)sizeof body, &type, &page);

    extra[0] = '\0';
    if (page) {
        /* Path=/ 让下一次访问页面时把次数带回来。HTTP 自己仍不保存客户状态。 */
        snprintf(extra, sizeof extra, "Set-Cookie: visit=%d; Path=/\r\n", req->visit);
    }
    if (strcmp(req->version, "HTTP/1.0") == 0 || strcmp(req->version, "HTTP/1.1") == 0) {
        ver = req->version;
    }
    n = snprintf(buf, (size_t)cap,
                 "%s %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "%s"
                 "Connection: %s\r\n"
                 "\r\n"
                 "%s",
                 ver, status, type, (int)strlen(body), extra,
                 req->persistent ? "keep-alive" : "close", body);
    if (n < 0 || n >= cap) {
        return -1;
    }
    return n;
}
