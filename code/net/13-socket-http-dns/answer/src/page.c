#include "page.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void page_fmt_us(long us, char *out, int cap) {
    if (us % 1000000L == 0) {
        snprintf(out, (size_t)cap, "%ld s", us / 1000000L);
    } else if (us % 1000L == 0) {
        snprintf(out, (size_t)cap, "%ld ms", us / 1000L);
    } else {
        snprintf(out, (size_t)cap, "%ld us", us);
    }
}

static int add(PageTrace *t, const char *fmt, ...) {
    va_list ap;
    int n;

    if (t->n >= PAGE_STEP_MAX) {
        return -1;
    }
    va_start(ap, fmt);
    n = vsnprintf(t->step[t->n].line, sizeof t->step[t->n].line, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof t->step[t->n].line) {
        return -1;
    }
    t->n++;
    return 0;
}

/* 每个新确认加 1 个 MSS。这一轮发满窗口时看起来才是翻倍；最后一轮往往发不满。 */
static int burst(PageTrace *t, int *cwnd, int mss, const char *what) {
    int left = mss;
    int rtts = 0;
    int first = 1;

    while (left > 0) {
        int n = *cwnd < left ? *cwnd : left;
        int next = *cwnd + n;
        int rc;

        if (first) {
            rc = add(t, "%s %d MSS：拥塞窗口 %d MSS，本轮发送 %d MSS，1 个 RTT，窗口变为 %d MSS。",
                     what, mss, *cwnd, n, next);
        } else {
            rc = add(t, "%s还剩 %d MSS：拥塞窗口 %d MSS，本轮发送 %d MSS，1 个 RTT，窗口变为 %d MSS。",
                     what, left, *cwnd, n, next);
        }
        if (rc != 0) {
            return -1;
        }
        *cwnd = next;
        left -= n;
        rtts++;
        first = 0;
    }
    return rtts;
}

static const char *tiny_non(const PageIn *in, PageTrace *t, int *spent) {
    int batches = 0;
    int html = in->from_request ? 1 : 2;

    if (in->objects > 0) {
        batches = (in->objects + in->parallel - 1) / in->parallel;
    }
    if (in->parallel == 1) {
        if (add(t, "非持久连接，串行。HTML 1 个，引用对象 %d 个。", in->objects) != 0) {
            return "步骤太多";
        }
    } else if (add(t, "非持久连接，最多 %d 条并行 TCP。HTML 1 个，引用对象 %d 个。",
                   in->parallel, in->objects) != 0) {
        return "步骤太多";
    }
    if (in->from_request) {
        if (add(t, "取 HTML：请求已经发出，响应 1 个 RTT。") != 0) {
            return "步骤太多";
        }
    } else if (add(t, "取 HTML：建立连接 1 个 RTT，请求并收到页面 1 个 RTT。") != 0) {
        return "步骤太多";
    }
    if (in->objects > 0 && in->parallel == 1) {
        if (add(t, "%d 个对象各建一条连接，每个 2 个 RTT，共 %d 个 RTT。",
                in->objects, 2 * in->objects) != 0) {
            return "步骤太多";
        }
    } else if (in->objects > 0 &&
               add(t, "%d 个对象分成 %d 批，每批 2 个 RTT，共 %d 个 RTT。",
                   in->objects, batches, 2 * batches) != 0) {
        return "步骤太多";
    }
    *spent = html + 2 * batches;
    return NULL;
}

static const char *persist_head(const PageIn *in, PageTrace *t) {
    if (in->from_request && in->parallel == 1) {
        if (add(t, "持久连接，非流水线。HTML 的响应 1 个 RTT，引用对象 %d 个。",
                in->objects) != 0) {
            return "步骤太多";
        }
        return NULL;
    }
    if (in->parallel == 1) {
        if (add(t, "持久连接，非流水线。HTML 1 个，引用对象 %d 个。", in->objects) != 0) {
            return "步骤太多";
        }
    } else if (add(t, "持久连接，非流水线，最多 %d 条并行 TCP。HTML 1 个，引用对象 %d 个。",
                   in->parallel, in->objects) != 0) {
        return "步骤太多";
    }
    if (in->from_request) {
        if (add(t, "页面响应：1 个 RTT。") != 0) {
            return "步骤太多";
        }
    } else if (add(t, "建立连接并取 HTML：2 个 RTT。") != 0) {
        return "步骤太多";
    }
    return NULL;
}

static const char *tiny_persist(const PageIn *in, PageTrace *t, int *spent) {
    int batches = 0;
    int html = in->from_request ? 1 : 2;
    const char *err = persist_head(in, t);

    if (err != NULL) {
        return err;
    }
    if (in->objects > 0) {
        batches = (in->objects + in->parallel - 1) / in->parallel;
    }
    if (in->objects > 0 && in->parallel == 1) {
        if (add(t, "%d 个对象各用 1 个 RTT，共 %d 个 RTT。", in->objects, in->objects) != 0) {
            return "步骤太多";
        }
    } else if (in->objects > 0 &&
               add(t, "%d 个对象分成 %d 批，每批 1 个 RTT，共 %d 个 RTT。",
                   in->objects, batches, batches) != 0) {
        return "步骤太多";
    }
    *spent = html + (in->parallel == 1 ? in->objects : batches);
    return NULL;
}

static const char *tiny_pipe(const PageIn *in, PageTrace *t, int *spent) {
    int html = in->from_request ? 1 : 2;
    int extra = in->objects > 0 ? 1 : 0;

    if (add(t, "持久连接，流水线。HTML 1 个，引用对象 %d 个。", in->objects) != 0) {
        return "步骤太多";
    }
    if (in->from_request) {
        if (add(t, "页面响应：1 个 RTT。") != 0) {
            return "步骤太多";
        }
    } else if (add(t, "建立连接并取 HTML：2 个 RTT。") != 0) {
        return "步骤太多";
    }
    if (extra && add(t, "%d 个对象的请求连续发出，一共 1 个 RTT。", in->objects) != 0) {
        return "步骤太多";
    }
    *spent = html + extra;
    return NULL;
}

static const char *tiny(const PageIn *in, PageTrace *t, int *spent) {
    if (in->mode == PAGE_NON) {
        return tiny_non(in, t, spent);
    }
    if (in->mode == PAGE_PERSIST) {
        return tiny_persist(in, t, spent);
    }
    return tiny_pipe(in, t, spent);
}

static const char *mss_one_conn(const PageIn *in, PageTrace *t, int *cwnd, int *used) {
    int i;

    for (i = 0; i < in->objects; i++) {
        int n;

        if (in->mode == PAGE_NON) {
            *cwnd = 1;
            if (add(t, "引用对象重新建立连接：1 个 RTT。") != 0) {
                return "步骤太多";
            }
            *used += 1;
        }
        n = burst(t, cwnd, in->obj_mss, "对象");
        if (n < 0) {
            return "步骤太多";
        }
        *used += n;
    }
    return NULL;
}

static const char *mss_objects(const PageIn *in, PageTrace *t, int *cwnd, int *used) {
    long long all;
    int n;

    if (in->objects == 0) {
        return NULL;
    }
    if (in->mode != PAGE_PIPE) {
        return mss_one_conn(in, t, cwnd, used);
    }
    all = (long long)in->objects * in->obj_mss;
    if (all > 1000000) {
        return "对象太大";
    }
    n = burst(t, cwnd, (int)all, "全部对象");
    if (n < 0) {
        return "步骤太多";
    }
    *used += n;
    return NULL;
}

static const char *mss_mode(const PageIn *in, PageTrace *t, int *spent) {
    int cwnd = 1;
    int used = 0;
    int n;
    const char *lead = "持久连接，非流水线。";
    const char *err;

    if (in->mode == PAGE_NON) {
        lead = "非持久连接，每条连接单独慢开始。";
    } else if (in->mode == PAGE_PIPE) {
        lead = "持久连接，流水线。";
    }
    /* 王道初始拥塞窗口是 1 个 MSS。按 RFC 6928 的 10，2022 的图像一轮就能发完。 */
    if (add(t, "%s初始拥塞窗口 1 MSS。", lead) != 0) {
        return "步骤太多";
    }
    if (!in->from_request) {
        if (add(t, "建立连接：1 个 RTT。") != 0) {
            return "步骤太多";
        }
        used++;
    }
    n = burst(t, &cwnd, in->html_mss, "HTML");
    if (n < 0) {
        return "步骤太多";
    }
    used += n;
    err = mss_objects(in, t, &cwnd, &used);
    if (err != NULL) {
        return err;
    }
    *spent = used;
    return NULL;
}

static const char *check_page(const PageIn *in) {
    if (in->dns < 0 || in->objects < 0 || in->parallel < 1) {
        return "参数不合法";
    }
    if (in->mode < PAGE_NON || in->mode > PAGE_PIPE) {
        return "先写 mode";
    }
    if (in->dns > 100000 || in->objects > 100000 || in->parallel > 1000) {
        return "数字太大";
    }
    if (in->html_mss < 0 || in->obj_mss < 0 || in->html_mss > 100000 || in->obj_mss > 100000) {
        return "MSS 不合法";
    }
    if (in->obj_mss > 0 && in->objects == 0) {
        return "没有引用对象，不要写 obj";
    }
    if (in->html_mss == 0 && in->obj_mss > 0) {
        return "写了 obj 就要写 html";
    }
    if (in->html_mss > 0 && in->objects > 0 && in->obj_mss <= 0) {
        return "按 MSS 计算时，每个引用对象都要有 obj";
    }
    if (in->html_mss > 0 && in->objects > 8) {
        return "按 MSS 计算时引用对象太多";
    }
    if (in->html_mss > 0 && in->parallel > 1) {
        return "按 MSS 计算时不能并行";
    }
    if (in->mode == PAGE_PIPE && in->parallel > 1) {
        return "流水线不用 parallel";
    }
    return NULL;
}

static const char *preamble(const PageIn *in, PageTrace *out) {
    if (add(out, "DNS：%d 个 RTT。", in->dns) != 0) {
        return "步骤太多";
    }
    if (in->rtt_us > 0) {
        char spec[32];

        page_fmt_us(in->rtt_us, spec, (int)sizeof spec);
        if (add(out, "往返时间：%s。", spec) != 0) {
            return "步骤太多";
        }
    }
    if (in->from_request &&
        add(out, "从已经发出的页面请求起算，连接建立不计入。") != 0) {
        return "步骤太多";
    }
    return NULL;
}

const char *page_trace(const PageIn *in, PageTrace *out) {
    const char *err;
    int spent = 0;

    err = check_page(in);
    if (err != NULL) {
        return err;
    }
    memset(out, 0, sizeof *out);
    err = preamble(in, out);
    if (err != NULL) {
        return err;
    }
    err = in->html_mss > 0 ? mss_mode(in, out, &spent) : tiny(in, out, &spent);
    if (err != NULL) {
        return err;
    }
    if (in->dns > 0 &&
        add(out, "HTTP 部分 %d 个 RTT，加上 DNS 共 %d 个 RTT。", spent, in->dns + spent) != 0) {
        return "步骤太多";
    }
    out->total = in->dns + spent;
    return NULL;
}
