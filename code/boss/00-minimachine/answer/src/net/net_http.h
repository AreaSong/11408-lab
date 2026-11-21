#ifndef NET_HTTP_H
#define NET_HTTP_H

/* mode：0 非持久，1 持久非流水线，2 流水线。from_req 为 1 时 HTML 只剩响应的 1 个 RTT。 */
int net_http_rtts(int refs, int mode, int parallel, int from_req);
/* 初始窗口 1 个 MSS。握手 1 个 RTT，之后每个确认把窗口加 1 个 MSS。 */
int net_http_slow(int html_mss, int obj_mss, int *rtts, int *cwnd);
int net_http_req(const char *text, int *persist, int *cookie);
int net_http_line(const char *id, char *out, int cap);

#endif
