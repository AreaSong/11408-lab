#ifndef FWDLAY_H
#define FWDLAY_H

typedef struct {
    long long bits;
    long long ns;
} LayTime;

/* store = 0：直通，只算目的 MAC 的 6 字节。store = 1：存储转发，算整帧。 */
int fwdlay_ns(int store, long long bps, long long frame_bytes, LayTime *out, char *err);

#endif
