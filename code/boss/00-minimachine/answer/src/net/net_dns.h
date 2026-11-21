#ifndef NET_DNS_H
#define NET_DNS_H

/* cache：0 无，1 主机自己有，2 本地域名服务器已经有 IP。recur 为 1 时本地只向上发 1 条。 */
int net_dns_count(int recur, int cache, int zones, int *host, int *local, int *total);
int net_dns_line(const char *id, char *out, int cap);

#endif
