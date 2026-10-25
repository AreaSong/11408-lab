#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>

#define FIB_MAX 32
#define IFACE_MAX 16

typedef struct {
    uint32_t network;
    int prefix;
    uint32_t nexthop;
    int direct;
    char iface[IFACE_MAX];
} Route;

typedef struct TrieNode {
    struct TrieNode *bit[2];
    int route;
} TrieNode;

typedef struct {
    TrieNode *root;
    Route item[FIB_MAX];
    int n;
} Fib;

void fib_init(Fib *fib);
void fib_clear(Fib *fib);

/* 1 新插入，2 同一前缀被替换，0 失败（原因写在 err）。 */
int fib_add(Fib *fib, uint32_t net, int prefix, int direct, uint32_t hop, const char *iface, char *err,
            int err_cap);

/* 没有匹配（含默认路由）时返回 -1。 */
int fib_lookup(const Fib *fib, uint32_t ip);

/* 把匹配的下标按前缀从短到长写进 out，返回条数。 */
int fib_matches(const Fib *fib, uint32_t ip, int *out, int cap);

#endif
