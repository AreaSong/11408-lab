#ifndef DNSQ_H
#define DNSQ_H

#define DNS_ITER 1
#define DNS_RECUR 2
#define DNS_CACHE_NONE 0
#define DNS_CACHE_HOST 1
#define DNS_CACHE_FINAL 2
#define DNS_CACHE_FROM 3
#define DNS_MAX_SERVER 6
#define DNS_STEP_MAX 24

#define DNS_NOTE 0
#define DNS_QUERY 1
#define DNS_RESP 2

typedef struct {
    char name[256];
    char server[DNS_MAX_SERVER][48];
    int nserver;
    int mode;
    int cache;
    char from_name[48];
} DnsIn;

typedef struct {
    char line[320];
    int kind;
    int from_host;
    int from_local;
} DnsStep;

typedef struct {
    char title[200];
    DnsStep step[DNS_STEP_MAX];
    int n;
    int queries;
    int responses;
    int host_q;
    int local_q;
} DnsTrace;

/* 成功返回 NULL。主机、本地域名服务器的条数只统计它们发出的查询。 */
const char *dns_trace(const DnsIn *in, DnsTrace *out);

#endif
