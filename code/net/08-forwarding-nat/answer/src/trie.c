#include "trie.h"

#include "addr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TrieNode *new_node(void) {
    TrieNode *node = calloc(1, sizeof *node);

    if (node != NULL) {
        node->route = -1;
    }
    return node;
}

void fib_init(Fib *fib) {
    memset(fib, 0, sizeof *fib);
    fib->root = new_node();
}

static void free_node(TrieNode *node) {
    if (node == NULL) {
        return;
    }
    free_node(node->bit[0]);
    free_node(node->bit[1]);
    free(node);
}

void fib_clear(Fib *fib) {
    free_node(fib->root);
    fib_init(fib);
}

static int set_err(char *err, int cap, const char *msg) {
    if (err != NULL && cap > 0) {
        snprintf(err, (size_t)cap, "%s", msg);
    }
    return 0;
}

static void store_route(Route *route, uint32_t net, int prefix, int direct, uint32_t hop,
                        const char *iface) {
    route->network = net;
    route->prefix = prefix;
    route->direct = direct;
    route->nexthop = hop;
    snprintf(route->iface, sizeof route->iface, "%s", iface);
}

static int replace_route(Fib *fib, uint32_t net, int prefix, int direct, uint32_t hop, const char *iface) {
    int i;

    for (i = 0; i < fib->n; i++) {
        if (fib->item[i].prefix == prefix && fib->item[i].network == net) {
            store_route(&fib->item[i], net, prefix, direct, hop, iface);
            return 1;
        }
    }
    return 0;
}

static TrieNode *walk_prefix(TrieNode *node, uint32_t net, int prefix) {
    int i;

    for (i = 0; i < prefix; i++) {
        int bit = (int)((net >> (31 - i)) & 1u);

        if (node->bit[bit] == NULL) {
            node->bit[bit] = new_node();
            if (node->bit[bit] == NULL) {
                return NULL;
            }
        }
        node = node->bit[bit];
    }
    return node;
}

int fib_add(Fib *fib, uint32_t net, int prefix, int direct, uint32_t hop, const char *iface, char *err,
            int err_cap) {
    TrieNode *node;
    int index;

    if (fib->root == NULL) {
        return set_err(err, err_cap, "Trie 没有建起来");
    }
    if (prefix < 0 || prefix > 32) {
        return set_err(err, err_cap, "前缀要在 0 到 32");
    }
    if (!ip_aligned(net, prefix)) {
        return set_err(err, err_cap, "目的网络的主机位不是 0");
    }
    if (!direct && hop == 0) {
        return set_err(err, err_cap, "下一跳不能是 0.0.0.0，直连路由不要写 via");
    }
    if (iface[0] == '\0') {
        return set_err(err, err_cap, "接口不能空");
    }
    if (replace_route(fib, net, prefix, direct, hop, iface)) {
        return 2;
    }
    if (fib->n >= FIB_MAX) {
        return set_err(err, err_cap, "路由表最多 32 条");
    }
    node = walk_prefix(fib->root, net, prefix);
    if (node == NULL) {
        return set_err(err, err_cap, "内存不足");
    }
    index = fib->n++;
    store_route(&fib->item[index], net, prefix, direct, hop, iface);
    node->route = index;
    return 1;
}

int fib_lookup(const Fib *fib, uint32_t ip) {
    const TrieNode *node = fib->root;
    int best;
    int i;

    if (node == NULL) {
        return -1;
    }
    best = node->route;
    for (i = 0; i < 32; i++) {
        node = node->bit[(ip >> (31 - i)) & 1u];
        if (node == NULL) {
            break;
        }
        if (node->route >= 0) {
            best = node->route;
        }
    }
    return best;
}

int fib_matches(const Fib *fib, uint32_t ip, int *out, int cap) {
    int i;
    int n = 0;
    int j;

    for (i = 0; i < fib->n; i++) {
        uint32_t mask = prefix_mask(fib->item[i].prefix);

        if ((ip & mask) == fib->item[i].network && n < cap) {
            out[n++] = i;
        }
    }
    for (i = 1; i < n; i++) {
        int key = out[i];

        j = i;
        while (j > 0 && fib->item[out[j - 1]].prefix > fib->item[key].prefix) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = key;
    }
    return n;
}
