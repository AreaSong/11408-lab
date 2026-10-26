#ifndef PAGE_H
#define PAGE_H

#define PAGE_NON 0
#define PAGE_PERSIST 1
#define PAGE_PIPE 2
#define PAGE_STEP_MAX 24

typedef struct {
    int dns;
    int objects;
    int parallel;
    int mode;
    int from_request;
    int html_mss;
    int obj_mss;
    long rtt_us;
} PageIn;

typedef struct {
    char line[192];
} PageStep;

typedef struct {
    PageStep step[PAGE_STEP_MAX];
    int n;
    int total;
} PageTrace;

void page_fmt_us(long us, char *out, int cap);

/* 成功返回 NULL。total 已把 DNS 的 RTT 算进去。 */
const char *page_trace(const PageIn *in, PageTrace *out);

#endif
