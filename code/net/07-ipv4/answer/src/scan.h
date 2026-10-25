#ifndef SCAN_H
#define SCAN_H

#include <stdio.h>

/* 失败时把原因写进 err，并返回 0。err 可以是 NULL。 */
int fail_msg(char *err, int cap, const char *msg);

const char *skip_space(const char *s);
int rest_empty(const char *s);

/* 取出下一个以空白分隔的词。没有词返回 0，词太长返回 -1，成功返回 1。 */
int take_token(const char **ps, char *out, int cap);

int token_eq(const char *tok, const char *lit);
int parse_u32(const char *tok, unsigned *out);

/* 读完这一行剩下的两位十六进制字节。至少一个。 */
int parse_hex_bytes(const char **ps, unsigned char *buf, int cap, int *n,
                    char *err, int err_cap);

#endif
