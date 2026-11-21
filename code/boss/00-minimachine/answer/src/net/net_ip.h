#ifndef NET_IP_H
#define NET_IP_H

#include <stdint.h>

/* 片偏移以 8 字节为单位。除最后一片外，数据长度必须是 8 的倍数。 */
#define NET_FRAG_MAX 8

typedef struct {
    int total;
    int data;
    int offset;
    int mf;
} NetFrag;

int net_ip_parse(const char *text, uint32_t *addr);
int net_ip_fmt(uint32_t addr, char *out, int cap);
int net_ip_cidr(const char *text, uint32_t *net, int *prefix);
uint32_t net_ip_mask(int prefix);
uint32_t net_ip_net(uint32_t addr, int prefix);
uint32_t net_ip_bcast(uint32_t addr, int prefix);
/* /32 记 1 台；/31 按 408 记 0 台。主机位超过 30 时放不进 int，返回 -1。 */
int net_ip_hosts(int prefix);
int net_ip_class(uint32_t addr);
int net_ip_private(uint32_t addr);
int net_ip_fixed(int base_pre, int sub_pre, int *each);
int net_ip_agg(const uint32_t *net, const int *pre, int n, uint32_t *out, int *out_pre, int *filled);
int net_ip_vlsm(uint32_t base, int base_pre, int blocks, uint32_t *nets, int *pres, int cap);
int net_ip_third(uint32_t known, int kp, uint32_t cand, int cp, int want, uint32_t *other, int *op,
                 int *ok);
/* df 为 1 且数据放不进 MTU 时返回 -2，路由器丢弃，不能切。 */
int net_ip_frag(int total, int hdr, int mtu, int df, NetFrag *out, int cap);
unsigned net_ip_cksum(const unsigned char *hdr, int len);
int net_ip_set(unsigned char hdr[20], int ttl, int change_src, uint32_t src);
int net_ip_2012(int sent[4], int *ns, int hands[4], int *nh, int pads[4], int *np, int *app,
                int *routers);

int net_cidr_line(const char *id, char *out, int cap);
int net_frag_line(const char *arg, char *out, int cap);
int net_hdr_line(const char *id, char *out, int cap);

#endif
