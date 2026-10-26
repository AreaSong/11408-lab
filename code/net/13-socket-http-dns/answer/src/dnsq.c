#include "dnsq.h"

#include <stdio.h>
#include <string.h>

static int push_note(DnsTrace *t, const char *text) {
    DnsStep *s;

    if (t->n >= DNS_STEP_MAX) {
        return -1;
    }
    s = &t->step[t->n];
    if (snprintf(s->line, sizeof s->line, "%s", text) >= (int)sizeof s->line) {
        return -1;
    }
    s->kind = DNS_NOTE;
    t->n++;
    return 0;
}

static int push_msg(DnsTrace *t, int kind, const char *from, const char *to, const char *action) {
    DnsStep *s;

    if (t->n >= DNS_STEP_MAX) {
        return -1;
    }
    s = &t->step[t->n];
    if (snprintf(s->line, sizeof s->line, "%s → %s：%s", from, to, action) >= (int)sizeof s->line) {
        return -1;
    }
    s->kind = kind;
    s->from_host = strcmp(from, "主机") == 0;
    s->from_local = strcmp(from, "本地域名服务器") == 0;
    t->n++;
    return 0;
}

static void tally(DnsTrace *t) {
    int i;

    t->queries = 0;
    t->responses = 0;
    t->host_q = 0;
    t->local_q = 0;
    for (i = 0; i < t->n; i++) {
        if (t->step[i].kind == DNS_QUERY) {
            t->queries++;
            t->host_q += t->step[i].from_host;
            t->local_q += t->step[i].from_local;
        } else if (t->step[i].kind == DNS_RESP) {
            t->responses++;
        }
    }
}

static void fill_title(const DnsIn *in, DnsTrace *t) {
    const char *mode = in->mode == DNS_RECUR ? "递归查询" : "迭代查询";

    if (in->cache == DNS_CACHE_FROM) {
        snprintf(t->title, sizeof t->title, "%s，从 %s 继续。域名 %s。", mode, in->from_name, in->name);
        return;
    }
    if (in->cache == DNS_CACHE_HOST) {
        snprintf(t->title, sizeof t->title, "%s，主机缓存。域名 %s。", mode, in->name);
        return;
    }
    if (in->cache == DNS_CACHE_FINAL) {
        snprintf(t->title, sizeof t->title, "%s，本地域名服务器缓存。域名 %s。", mode, in->name);
        return;
    }
    snprintf(t->title, sizeof t->title, "%s，无缓存。域名 %s。", mode, in->name);
}

static const char *ask(char *action, int cap, const char *name) {
    if (snprintf(action, (size_t)cap, "查询 %s", name) >= cap) {
        return "域名太长";
    }
    return NULL;
}

static const char *walk_recursive(const DnsIn *in, DnsTrace *t, int begin) {
    char action[280];
    const char *err;
    int i;
    int n = in->nserver;

    err = ask(action, (int)sizeof action, in->name);
    if (err != NULL) {
        return err;
    }
    if (push_msg(t, DNS_QUERY, "主机", "本地域名服务器", action) != 0 ||
        push_msg(t, DNS_QUERY, "本地域名服务器", in->server[begin], action) != 0) {
        return "步骤太多";
    }
    for (i = begin; i < n - 1; i++) {
        if (push_msg(t, DNS_QUERY, in->server[i], in->server[i + 1], action) != 0) {
            return "步骤太多";
        }
    }
    for (i = n - 1; i > begin; i--) {
        if (push_msg(t, DNS_RESP, in->server[i], in->server[i - 1], "回答 IP") != 0) {
            return "步骤太多";
        }
    }
    if (push_msg(t, DNS_RESP, in->server[begin], "本地域名服务器", "回答 IP") != 0 ||
        push_msg(t, DNS_RESP, "本地域名服务器", "主机", "回答 IP") != 0) {
        return "步骤太多";
    }
    return NULL;
}

static const char *walk_iterative(const DnsIn *in, DnsTrace *t, int begin) {
    char action[280];
    char refer[80];
    const char *err;
    int i;

    err = ask(action, (int)sizeof action, in->name);
    if (err != NULL) {
        return err;
    }
    if (push_msg(t, DNS_QUERY, "主机", "本地域名服务器", action) != 0) {
        return "步骤太多";
    }
    for (i = begin; i < in->nserver; i++) {
        const char *back = "回答 IP";

        if (i + 1 < in->nserver) {
            if (snprintf(refer, sizeof refer, "去问 %s", in->server[i + 1]) >= (int)sizeof refer) {
                return "服务器名字太长";
            }
            back = refer;
        }
        if (push_msg(t, DNS_QUERY, "本地域名服务器", in->server[i], action) != 0 ||
            push_msg(t, DNS_RESP, in->server[i], "本地域名服务器", back) != 0) {
            return "步骤太多";
        }
    }
    if (push_msg(t, DNS_RESP, "本地域名服务器", "主机", "回答 IP") != 0) {
        return "步骤太多";
    }
    return NULL;
}

static int find_server(const DnsIn *in, const char *name) {
    int i;

    for (i = 0; i < in->nserver; i++) {
        if (strcmp(in->server[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static const char *check_dns(const DnsIn *in, int *begin) {
    *begin = 0;
    if (in->name[0] == '\0') {
        return "先写 name";
    }
    if (in->mode != DNS_ITER && in->mode != DNS_RECUR) {
        return "先写 mode";
    }
    if (in->cache == DNS_CACHE_HOST || in->cache == DNS_CACHE_FINAL) {
        return NULL;
    }
    if (in->nserver <= 0) {
        return "先写 server";
    }
    if (in->cache == DNS_CACHE_FROM) {
        *begin = find_server(in, in->from_name);
        if (*begin < 0) {
            return "cache from 的服务器不在列表里";
        }
    }
    return NULL;
}

static const char *cached(const DnsIn *in, DnsTrace *out) {
    char action[280];

    if (in->cache == DNS_CACHE_HOST) {
        if (snprintf(action, sizeof action, "主机缓存已有 %s，不再询问域名服务器。", in->name) >=
            (int)sizeof action) {
            return "域名太长";
        }
        if (push_note(out, action) != 0) {
            return "步骤太多";
        }
        return NULL;
    }
    if (ask(action, (int)sizeof action, in->name) != NULL) {
        return "域名太长";
    }
    if (push_msg(out, DNS_QUERY, "主机", "本地域名服务器", action) != 0 ||
        push_msg(out, DNS_RESP, "本地域名服务器", "主机", "回答 IP（本地缓存）") != 0) {
        return "步骤太多";
    }
    return NULL;
}

const char *dns_trace(const DnsIn *in, DnsTrace *out) {
    int begin = 0;
    const char *err = check_dns(in, &begin);

    if (err != NULL) {
        return err;
    }
    memset(out, 0, sizeof *out);
    fill_title(in, out);
    if (in->cache == DNS_CACHE_HOST || in->cache == DNS_CACHE_FINAL) {
        err = cached(in, out);
    } else if (in->mode == DNS_RECUR) {
        err = walk_recursive(in, out, begin);
    } else {
        err = walk_iterative(in, out, begin);
    }
    if (err != NULL) {
        return err;
    }
    tally(out);
    return NULL;
}
