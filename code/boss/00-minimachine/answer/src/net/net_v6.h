#ifndef NET_V6_H
#define NET_V6_H

/* RFC 5952：只压最长的一段连续 0，一样长就压最左边。单独一个 0 不压。 */
int net_v6_compress(const char *text, char *out, int cap);
int net_v6_expand(const char *text, char *out, int cap);

#endif
