#include "net/net_http.h"

#include <stdio.h>
#include <string.h>

int net_http_rtts(int refs, int mode, int parallel, int from_req) {
    int html = from_req ? 1 : 2;
    int batches;
    if (refs < 0 || parallel < 1 || mode < 0 || mode > 2) {
        return -1;
    }
    if (mode == 2) {
        /* 流水线把引用对象的请求一起发出去。收完内容不算挥手。 */
        return refs > 0 ? html + 1 : html;
    }
    if (mode == 1) {
        return html + refs;
    }
    batches = refs == 0 ? 0 : (refs + parallel - 1) / parallel;
    return html + 2 * batches;
}

static int burst(int *cwnd, int left) {
    int rtts = 0;
    while (left > 0) {
        int n = *cwnd < left ? *cwnd : left;
        *cwnd += n;
        left -= n;
        rtts++;
    }
    return rtts;
}

int net_http_slow(int html_mss, int obj_mss, int *rtts, int *cwnd) {
    int window = 1;
    int spent = 1;
    if (html_mss <= 0 || obj_mss < 0 || rtts == NULL || cwnd == NULL) {
        return -1;
    }
    spent += burst(&window, html_mss);
    spent += burst(&window, obj_mss);
    *rtts = spent;
    *cwnd = window;
    return 0;
}

int net_http_req(const char *text, int *persist, int *cookie) {
    if (text == NULL || persist == NULL || cookie == NULL) {
        return -1;
    }
    /* Connection: close 压过 HTTP/1.1 默认的持续连接。 */
    *persist = strstr(text, "Connection: close") == NULL;
    *cookie = strstr(text, "Cookie:") != NULL;
    return 0;
}

int net_http_line(const char *id, char *out, int cap) {
    int n;
    int rtts = 0;
    int cwnd = 0;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2011-47") == 0) {
        n = snprintf(out, (size_t)cap, "rtt=%d", net_http_rtts(5, 1, 1, 1));
    } else if (strcmp(id, "2015-40") == 0) {
        static const char *req = "GET /index.html HTTP/1.1\r\nHost: www.example.com\r\n"
                                  "Connection: close\r\nCookie: 123\r\n\r\n";
        int persist = 0;
        int cookie = 0;
        if (net_http_req(req, &persist, &cookie) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "persist=%d cookie=%d wrong=C", persist, cookie);
    } else if (strcmp(id, "2022-40") == 0) {
        if (net_http_slow(1, 3, &rtts, &cwnd) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "rtt=%d ms=%d letter=B", rtts, rtts * 10);
    } else if (strcmp(id, "2024-40") == 0) {
        n = snprintf(out, (size_t)cap, "rtt=%d letter=D", net_http_rtts(7, 0, 1, 0));
    } else if (strcmp(id, "modes") == 0) {
        n = snprintf(out, (size_t)cap, "non=%d persist=%d pipe=%d", net_http_rtts(7, 0, 1, 0),
                     net_http_rtts(7, 1, 1, 0), net_http_rtts(7, 2, 1, 0));
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
