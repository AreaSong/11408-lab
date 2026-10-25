#ifndef FRAME_H
#define FRAME_H

#include "bits.h"

#define FRAME_FLAG "01111110"
#define FRAME_FLAG_LEN 8
#define FRAME_MAX_MARK 80
#define FRAME_MAX_COUNT 64

typedef struct {
    int count;
    unsigned char data[255];
    int data_len;
} CountFrame;

int count_send(const ByteStr *payload, ByteStr *frame, char *err, int err_cap);
int count_recv(const ByteStr *in, CountFrame *frames, int cap, int *nframes,
               char *err, int err_cap);

/* 需要转义时返回 2，否则返回 1。第二个字节是原字节异或 0x20。 */
int byte_escape(unsigned char b, unsigned char out[2]);
int byte_stuff(const ByteStr *in, ByteStr *out, char *err, int err_cap);
int byte_unstuff(const ByteStr *in, ByteStr *out, char *err, int err_cap);

int bit_stuff(const char *in, int n, char *out, int cap, int *out_len,
              int *after, int *ninsert, char *err, int err_cap);
int bit_unstuff(const char *in, int n, char *out, int cap, int *out_len,
                int *deleted, int *ndeleted, char *err, int err_cap);
int bit_at_flag(const char *s, int n, int at);
int bit_split(const char *in, int n, BitStr *bodies, int cap, int *nframes,
              char *err, int err_cap);

int parity_ones(const char *bits, int n);
int parity_make(const char *data, int n, int odd, char *out, int cap, int *out_len,
                char *err, int err_cap);
int parity_check(const char *code, int n, int odd, int *ones, int *ok);

#endif
