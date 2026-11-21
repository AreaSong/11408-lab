#include "net/net_dns.h"

#include <stdio.h>
#include <string.h>

int net_dns_count(int recur, int cache, int zones, int *host, int *local, int *total) {
    if (host == NULL || local == NULL || total == NULL || zones < 1) {
        return -1;
    }
    if (cache == 1) {
        *host = 0;
        *local = 0;
        *total = 0;
        return 0;
    }
    if (cache == 2) {
        *host = 1;
        *local = 0;
        *total = 2;
        return 0;
    }
    if (cache != 0) {
        return -1;
    }
    /* 主机到本地总是一来一回。递归时本地只问根，后面的跳由根这条链去问。 */
    *host = 1;
    *local = recur ? 1 : zones;
    *total = 2 * (1 + zones);
    return 0;
}

int net_dns_line(const char *id, char *out, int cap) {
    int host = 0;
    int local = 0;
    int total = 0;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2010-40") == 0) {
        if (net_dns_count(1, 0, 3, &host, &local, &total) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "host=%d local=%d total=%d letter=A", host, local, total);
    } else if (strcmp(id, "2016-40") == 0) {
        int most = 0;
        if (net_dns_count(0, 1, 4, &host, &local, &total) != 0 ||
            net_dns_count(0, 0, 4, &host, &most, &total) != 0) {
            return -1;
        }
        /* 问的是 201.1.1.1 自己发出的查询。www 不再单独占一台服务器。 */
        n = snprintf(out, (size_t)cap, "min=%d max=%d letter=C", local, most);
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
