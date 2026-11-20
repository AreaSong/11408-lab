#ifndef BYTES_H
#define BYTES_H

#include <stdio.h>

unsigned short read_be16(const unsigned char *p);
unsigned int read_be32(const unsigned char *p);
unsigned short read_le16(const unsigned char *p);
unsigned int read_le32(const unsigned char *p);
void write_be16(unsigned char *p, unsigned short v);
void write_be32(unsigned char *p, unsigned int v);
void write_le16(unsigned char *p, unsigned short v);
void write_le32(unsigned char *p, unsigned int v);

int parse_mac(const char *text, unsigned char mac[6]);
int parse_ipv4(const char *text, unsigned char ip[4]);
int parse_u32(const char *text, unsigned int *out);
int parse_tcp_flags(const char *text, unsigned char *flags);

/* 把一行里的 \r \n \t \\ \xHH 还原成字节。out_len 不含结尾。 */
int unescape(const char *text, unsigned char *out, int cap, int *out_len,
             char *err, int err_cap);

void format_mac(const unsigned char mac[6], char *out, int cap);
void format_ipv4(const unsigned char ip[4], char *out, int cap);

/* 读满 n 个两位十六进制字节。1 成功，0 提前结束，-1 格式错。bad_index 从 1 起。 */
int read_hex_bytes(FILE *fp, unsigned char *out, int n, int *bad_index);

#endif
