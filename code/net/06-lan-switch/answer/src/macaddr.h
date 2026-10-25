#ifndef MACADDR_H
#define MACADDR_H

#define MAC_LEN 6
#define MAC_TEXT 18

int mac_parse(const char *text, unsigned char mac[MAC_LEN], char *err);
void mac_fmt(const unsigned char mac[MAC_LEN], char out[MAC_TEXT]);
int mac_eq(const unsigned char a[MAC_LEN], const unsigned char b[MAC_LEN]);
int mac_is_group(const unsigned char mac[MAC_LEN]);
int mac_is_broadcast(const unsigned char mac[MAC_LEN]);

#endif
