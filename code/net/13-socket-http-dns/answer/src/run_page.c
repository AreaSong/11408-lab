#include "run_page.h"

#include "line.h"
#include "page.h"

#include <stdio.h>
#include <string.h>

static PageIn page;
static int ready;
static int mode_set;

static void page_init(PageIn *p) {
    memset(p, 0, sizeof *p);
    p->parallel = 1;
}

static PageIn *state(void) {
    if (!ready) {
        page_init(&page);
        ready = 1;
    }
    return &page;
}

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static int parse_rtt(const char *s, long *us) {
    long v = 0;
    const char *p = s;

    if (*p < '0' || *p > '9') {
        return -1;
    }
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 1000000L) {
            return -1;
        }
        p++;
    }
    if (strcmp(p, "ms") == 0 && v > 0) {
        *us = v * 1000L;
        return 0;
    }
    if (strcmp(p, "s") == 0 && v > 0) {
        *us = v * 1000000L;
        return 0;
    }
    return -1;
}

static int set_u(char **ps, int *dst, int zero_ok, const char *what) {
    char tok[32];
    int v = 0;

    if (take_need(ps, tok, (int)sizeof tok, what) != 0 || need_end(*ps) != 0) {
        return -1;
    }
    if (parse_u(tok, &v, 100000) != 0 || (!zero_ok && v <= 0)) {
        return reject(what);
    }
    *dst = v;
    return 0;
}

static int set_mode(PageIn *p, char **ps) {
    char tok[32];

    if (take_need(ps, tok, (int)sizeof tok, "mode 后面写 nonpersist、persist 或 pipeline") != 0) {
        return -1;
    }
    if (need_end(*ps) != 0) {
        return -1;
    }
    if (strcmp(tok, "nonpersist") == 0) {
        p->mode = PAGE_NON;
    } else if (strcmp(tok, "persist") == 0) {
        p->mode = PAGE_PERSIST;
    } else if (strcmp(tok, "pipeline") == 0) {
        p->mode = PAGE_PIPE;
    } else {
        return reject("mode 只能是 nonpersist、persist 或 pipeline");
    }
    mode_set = 1;
    return 0;
}

static int set_from(PageIn *p, char **ps) {
    char tok[32];

    if (take_need(ps, tok, (int)sizeof tok, "from 后面写 request 或 tcp") != 0) {
        return -1;
    }
    if (need_end(*ps) != 0) {
        return -1;
    }
    if (strcmp(tok, "request") == 0) {
        p->from_request = 1;
    } else if (strcmp(tok, "tcp") == 0) {
        p->from_request = 0;
    } else {
        return reject("from 只能是 request 或 tcp");
    }
    return 0;
}

static int print_result(const PageIn *p, int total) {
    if (p->rtt_us > 0) {
        char spec[32];
        long long us = (long long)total * p->rtt_us;

        if (us <= 0 || us > 2000000000LL) {
            return reject("时间太大");
        }
        page_fmt_us((long)us, spec, (int)sizeof spec);
        printf("结果：%d 个 RTT，%s\n", total, spec);
    } else {
        printf("结果：%d 个 RTT\n", total);
    }
    return 0;
}

static int cmd_go(PageIn *p) {
    PageTrace tr;
    const char *err;
    int i;

    if (!mode_set) {
        p->mode = -1;
    }
    err = page_trace(p, &tr);
    if (err != NULL) {
        return reject(err);
    }
    output_begin();
    for (i = 0; i < tr.n; i++) {
        printf("%s\n", tr.step[i].line);
    }
    return print_result(p, tr.total);
}

static int dispatch(PageIn *p, char *cmd, char **ps) {
    if (strcmp(cmd, "dns") == 0) {
        return set_u(ps, &p->dns, 1, "dns 要一个非负整数");
    }
    if (strcmp(cmd, "objects") == 0) {
        return set_u(ps, &p->objects, 1, "objects 要一个非负整数");
    }
    if (strcmp(cmd, "parallel") == 0) {
        return set_u(ps, &p->parallel, 0, "parallel 要一个正整数");
    }
    if (strcmp(cmd, "html") == 0) {
        return set_u(ps, &p->html_mss, 0, "html 要一个正整数，单位是 MSS");
    }
    if (strcmp(cmd, "obj") == 0) {
        return set_u(ps, &p->obj_mss, 0, "obj 要一个正整数，单位是 MSS");
    }
    if (strcmp(cmd, "mode") == 0) {
        return set_mode(p, ps);
    }
    if (strcmp(cmd, "from") == 0) {
        return set_from(p, ps);
    }
    if (strcmp(cmd, "reset") == 0) {
        if (need_end(*ps) != 0) {
            return -1;
        }
        page_init(p);
        mode_set = 0;
        return 0;
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(*ps) != 0) {
            return -1;
        }
        return cmd_go(p);
    }
    return reject("不认识的命令");
}

int run_page(char *line) {
    PageIn *p = state();
    char cmd[32];
    char tok[32];
    char *cursor = line;

    if (take_need(&cursor, cmd, (int)sizeof cmd, "缺少命令") != 0) {
        return -1;
    }
    if (strcmp(cmd, "rtt") == 0) {
        if (take_need(&cursor, tok, (int)sizeof tok, "rtt 后面写时间，例如 10ms") != 0) {
            return -1;
        }
        if (need_end(cursor) != 0 || parse_rtt(tok, &p->rtt_us) != 0) {
            return reject("时间写成 10ms 或 1s");
        }
        return 0;
    }
    return dispatch(p, cmd, &cursor);
}
