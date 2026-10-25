#ifndef IP_H
#define IP_H

#include <stdint.h>

typedef struct {
    uint32_t base;
    int prefix;
} Block;

/* A～E 是分类地址。0/8、127/8 和全 1 单独标出来，不当成可分配的 A 类或 E 类。 */
typedef enum {
    IP_KIND_A = 0,
    IP_KIND_B,
    IP_KIND_C,
    IP_KIND_D,
    IP_KIND_E,
    IP_KIND_THIS,
    IP_KIND_LOOP,
    IP_KIND_LIMITED
} IpKind;

typedef enum {
    IP_SCOPE_PUBLIC = 0,
    IP_SCOPE_PRIVATE,
    IP_SCOPE_SPECIAL
} IpScope;

int ip_parse(const char *text, uint32_t *out);
void ip_fmt(uint32_t ip, char *out, int cap);
void block_fmt(Block b, char *out, int cap);

uint32_t prefix_mask(int prefix);
int mask_to_prefix(uint32_t mask, int *prefix);
uint32_t ip_network(uint32_t addr, int prefix);
uint32_t ip_broadcast(uint32_t addr, int prefix);
int ip_aligned(uint32_t addr, int prefix);

/* 块里的地址个数。prefix 必须在 0～32。 */
uint64_t ip_space(int prefix);

/*
 * 可分配主机数。王道：主机号不能全 0（网络）也不能全 1（广播）。
 * /32 是主机路由，这一个地址可以分配。/31 两个地址分别是全 0 和全 1，按王道是 0。
 */
uint64_t ip_usable(int prefix);

uint32_t block_last(Block b);
int block_aligned(Block b);

void ip_classify(uint32_t addr, IpKind *kind, IpScope *scope);
const char *ip_kind_name(IpKind kind);
const char *ip_proto_name(unsigned protocol);

#endif
