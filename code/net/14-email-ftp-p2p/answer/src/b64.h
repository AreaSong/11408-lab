#ifndef B64_H
#define B64_H

/* MIME 用 Base64 把任意字节编成 7 位 ASCII。返回写入的字符数，不含结尾的 0。 */
int b64_encode(const unsigned char *in, int n, char *out, int cap);
int b64_decode(const char *in, unsigned char *out, int cap);

#endif
