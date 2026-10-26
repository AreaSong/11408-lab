#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char host[256];
    char cookie[256];
    int persistent;
    int has_cookie;
    int visit;
} HttpReq;

/* 成功返回 0。visit 只对 / 和 /index.html 有意义，从 1 起，Cookie 里的 visit 再加 1。 */
int http_parse(const char *raw, HttpReq *out);

/* 把响应写进 buf，返回字节数。放不下返回 -1。 */
int http_reply(const HttpReq *req, char *buf, int cap);

#endif
