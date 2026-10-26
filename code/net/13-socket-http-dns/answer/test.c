#include "dnsq.h"
#include "http.h"
#include "page.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int trace_has(const PageTrace *tr, const char *s) {
    int i;

    for (i = 0; i < tr->n; i++) {
        if (strstr(tr->step[i].line, s) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int page_total(int dns, int objects, int parallel, int mode, int from_request, int html,
                      int obj, long rtt_us) {
    PageIn in;
    PageTrace tr;

    memset(&in, 0, sizeof in);
    in.dns = dns;
    in.objects = objects;
    in.parallel = parallel;
    in.mode = mode;
    in.from_request = from_request;
    in.html_mss = html;
    in.obj_mss = obj;
    in.rtt_us = rtt_us;
    CHECK(page_trace(&in, &tr) == NULL);
    return tr.total;
}

static void check_page(void) {
    PageIn in;
    PageTrace tr;

    CHECK_EQ(page_total(0, 7, 1, PAGE_NON, 0, 0, 0, 0), 16);
    CHECK_EQ(page_total(0, 7, 1, PAGE_PERSIST, 0, 0, 0, 0), 9);
    CHECK_EQ(page_total(0, 7, 1, PAGE_PIPE, 0, 0, 0, 0), 3);
    CHECK_EQ(page_total(0, 5, 1, PAGE_PERSIST, 1, 0, 0, 0), 6);
    CHECK_EQ(page_total(0, 8, 6, PAGE_NON, 0, 0, 0, 0), 6);
    CHECK_EQ(page_total(4, 7, 1, PAGE_NON, 0, 0, 0, 0), 20);
    CHECK_EQ(page_total(0, 1, 1, PAGE_PERSIST, 0, 1, 3, 10000), 4);

    memset(&in, 0, sizeof in);
    in.objects = 1;
    in.parallel = 1;
    in.mode = PAGE_PERSIST;
    in.html_mss = 1;
    in.obj_mss = 3;
    CHECK(page_trace(&in, &tr) == NULL);
    CHECK(trace_has(&tr, "本轮发送 2 MSS"));
    CHECK(trace_has(&tr, "窗口变为 5 MSS"));
}

static void fill_2016(DnsIn *in) {
    memset(in, 0, sizeof *in);
    snprintf(in->name, sizeof in->name, "www.abc.xyz.com");
    snprintf(in->server[0], sizeof in->server[0], "根");
    snprintf(in->server[1], sizeof in->server[1], "com");
    snprintf(in->server[2], sizeof in->server[2], "xyz.com");
    snprintf(in->server[3], sizeof in->server[3], "abc.xyz.com");
    in->nserver = 4;
    in->mode = DNS_ITER;
}

static void check_dns(void) {
    DnsIn in;
    DnsTrace tr;

    memset(&in, 0, sizeof in);
    snprintf(in.name, sizeof in.name, "www.other.net");
    snprintf(in.server[0], sizeof in.server[0], "根");
    snprintf(in.server[1], sizeof in.server[1], "顶级");
    snprintf(in.server[2], sizeof in.server[2], "权威");
    in.nserver = 3;
    in.mode = DNS_RECUR;
    CHECK(dns_trace(&in, &tr) == NULL);
    CHECK_EQ(tr.host_q, 1);
    CHECK_EQ(tr.local_q, 1);
    CHECK_EQ(tr.queries + tr.responses, 8);

    in.mode = DNS_ITER;
    CHECK(dns_trace(&in, &tr) == NULL);
    CHECK_EQ(tr.host_q, 1);
    CHECK_EQ(tr.local_q, 3);
    CHECK_EQ(tr.queries + tr.responses, 8);

    fill_2016(&in);
    in.cache = DNS_CACHE_HOST;
    CHECK(dns_trace(&in, &tr) == NULL);
    CHECK_EQ(tr.local_q, 0);
    CHECK_EQ(tr.queries + tr.responses, 0);

    in.cache = DNS_CACHE_NONE;
    CHECK(dns_trace(&in, &tr) == NULL);
    CHECK_EQ(tr.local_q, 4);
    CHECK_EQ(tr.queries + tr.responses, 10);
}

static void check_http(void) {
    HttpReq req;
    char buf[2048];
    const char *exam =
        "GET /index.html HTTP/1.1\r\n"
        "Host: www.test.edu.cn\r\n"
        "Connection: Close\r\n"
        "Cookie: 123456\r\n"
        "\r\n";
    const char *again =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Cookie: visit=3\r\n"
        "\r\n";
    int n;

    CHECK(http_parse(exam, &req) == 0);
    CHECK_EQ(req.persistent, 0);
    CHECK_EQ(req.has_cookie, 1);
    CHECK_STR(req.host, "www.test.edu.cn");
    CHECK_STR(req.path, "/index.html");
    CHECK_EQ(req.visit, 1);

    CHECK(http_parse(again, &req) == 0);
    CHECK_EQ(req.persistent, 1);
    CHECK_EQ(req.visit, 4);
    n = http_reply(&req, buf, (int)sizeof buf);
    CHECK(n > 0);
    CHECK(strstr(buf, "Set-Cookie: visit=4; Path=/") != NULL);
    CHECK(strstr(buf, "第 4 次访问") != NULL);
    CHECK(strstr(buf, "Connection: keep-alive") != NULL);
}

int main(void) {
    check_page();
    check_dns();
    check_http();
    return TEST_SUMMARY();
}
