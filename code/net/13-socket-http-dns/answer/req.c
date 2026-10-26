#include "http.h"
#include "prompt.h"

#include <stdio.h>

static void show(const HttpReq *req) {
    printf("方法：%s\n", req->method);
    printf("路径：%s\n", req->path);
    printf("版本：%s\n", req->version);
    printf("主机：%s\n", req->host[0] != '\0' ? req->host : "（无）");
    printf("连接：%s\n", req->persistent ? "持续" : "非持续");
    printf("Cookie：%s\n", req->has_cookie ? "有" : "无");
    printf("结果：%s，%s\n", req->persistent ? "持续连接" : "非持续连接",
           req->has_cookie ? "有 Cookie" : "无 Cookie");
}

int main(void) {
    char buf[8192];
    size_t n = 0;
    HttpReq req;

    prompt("请输入 HTTP 请求报文，Ctrl+D 结束：");
    while (n + 1 < sizeof buf) {
        size_t got = fread(buf + n, 1, sizeof buf - 1 - n, stdin);

        n += got;
        if (got == 0) {
            break;
        }
    }
    buf[n] = '\0';
    if (http_parse(buf, &req) != 0) {
        fprintf(stderr, "请求报文不完整\n");
        return 1;
    }
    show(&req);
    return 0;
}
