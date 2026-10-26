#include "run_dns.h"

#include "dnsq.h"
#include "line.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static DnsIn dns;
static int ready;

static DnsIn *state(void) {
    if (!ready) {
        memset(&dns, 0, sizeof dns);
        ready = 1;
    }
    return &dns;
}

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static int good_name(const char *s) {
    int n = (int)strlen(s);
    int i;

    if (n < 1 || n > 253) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];

        if (!isalnum(c) && c != '.' && c != '-') {
            return 0;
        }
    }
    return 1;
}

static int set_name(DnsIn *in, char **ps) {
    char tok[256];

    if (take_need(ps, tok, (int)sizeof tok, "name 后面写域名") != 0 || need_end(*ps) != 0) {
        return -1;
    }
    if (!good_name(tok)) {
        return reject("域名只能有字母、数字、点和连字符");
    }
    snprintf(in->name, sizeof in->name, "%s", tok);
    return 0;
}

static int add_server(DnsIn *in, char **ps) {
    char tok[48];

    if (in->nserver >= DNS_MAX_SERVER) {
        return reject("最多 6 台服务器");
    }
    if (take_need(ps, tok, (int)sizeof tok, "server 后面写名字") != 0 || need_end(*ps) != 0) {
        return -1;
    }
    snprintf(in->server[in->nserver], sizeof in->server[in->nserver], "%s", tok);
    in->nserver++;
    return 0;
}

static int set_mode(DnsIn *in, char **ps) {
    char tok[32];

    if (take_need(ps, tok, (int)sizeof tok, "mode 后面写 recursive 或 iterative") != 0) {
        return -1;
    }
    if (need_end(*ps) != 0) {
        return -1;
    }
    if (strcmp(tok, "recursive") == 0) {
        in->mode = DNS_RECUR;
    } else if (strcmp(tok, "iterative") == 0) {
        in->mode = DNS_ITER;
    } else {
        return reject("mode 只能是 recursive 或 iterative");
    }
    return 0;
}

static int set_cache(DnsIn *in, char **ps) {
    char tok[32];

    if (take_need(ps, tok, (int)sizeof tok, "cache 后面写 none、host、final 或 from") != 0) {
        return -1;
    }
    if (strcmp(tok, "none") == 0) {
        in->cache = DNS_CACHE_NONE;
    } else if (strcmp(tok, "host") == 0) {
        in->cache = DNS_CACHE_HOST;
    } else if (strcmp(tok, "final") == 0) {
        in->cache = DNS_CACHE_FINAL;
    } else if (strcmp(tok, "from") == 0) {
        if (take_need(ps, in->from_name, (int)sizeof in->from_name, "cache from 后面写服务器") != 0) {
            return -1;
        }
        in->cache = DNS_CACHE_FROM;
    } else {
        return reject("cache 只能是 none、host、final 或 from");
    }
    return need_end(*ps);
}

static int cmd_go(const DnsIn *in) {
    DnsTrace tr;
    const char *err = dns_trace(in, &tr);
    int i;

    if (err != NULL) {
        return reject(err);
    }
    output_begin();
    printf("%s\n", tr.title);
    for (i = 0; i < tr.n; i++) {
        printf("%d. %s\n", i + 1, tr.step[i].line);
    }
    printf("查询 %d 条，响应 %d 条。\n", tr.queries, tr.responses);
    printf("结果：主机 %d 条，本地域名服务器 %d 条，共 %d 条\n", tr.host_q, tr.local_q,
           tr.queries + tr.responses);
    return 0;
}

int run_dns(char *line) {
    DnsIn *in = state();
    char cmd[32];
    char *cursor = line;

    if (take_need(&cursor, cmd, (int)sizeof cmd, "缺少命令") != 0) {
        return -1;
    }
    if (strcmp(cmd, "name") == 0) {
        return set_name(in, &cursor);
    }
    if (strcmp(cmd, "server") == 0) {
        return add_server(in, &cursor);
    }
    if (strcmp(cmd, "mode") == 0) {
        return set_mode(in, &cursor);
    }
    if (strcmp(cmd, "cache") == 0) {
        return set_cache(in, &cursor);
    }
    if (strcmp(cmd, "reset") == 0) {
        if (need_end(cursor) != 0) {
            return -1;
        }
        memset(in, 0, sizeof *in);
        return 0;
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(cursor) != 0) {
            return -1;
        }
        return cmd_go(in);
    }
    return reject("不认识的命令");
}
