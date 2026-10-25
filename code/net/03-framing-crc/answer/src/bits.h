#ifndef BITS_H
#define BITS_H

#define LAB_BIT_MAX 256
#define LAB_BIT_CAP 400
#define LAB_BYTE_MAX 128
#define LAB_BYTE_CAP 320

typedef struct {
    char bit[LAB_BIT_CAP];
    int len;
} BitStr;

typedef struct {
    unsigned char byte[LAB_BYTE_CAP];
    int len;
} ByteStr;

void lab_err(char *err, int cap, const char *msg);

/* 跳过空格。空串成功，长度为 0，由调用方决定是否允许。 */
int parse_bits(const char *text, BitStr *out, char *err, int err_cap);
int parse_bytes(const char *text, ByteStr *out, char *err, int err_cap);
int format_bytes(const unsigned char *b, int n, char *out, int cap);

#endif
