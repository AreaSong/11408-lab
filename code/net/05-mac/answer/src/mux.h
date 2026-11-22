#ifndef MUX_H
#define MUX_H

/* 同步 TDM 按位置认人，时隙里只有数据。STDM 只发有数据的用户，每个时隙还要地址。 */

typedef struct {
    long long useful;
    long long total;
} MuxUse;

/* 1 表示 users、busy、data、addr 都在允许范围内。 */
int mux_range(int users, int busy, int data, int addr);
void mux_tdm(int users, int busy, int data, MuxUse *out);
void mux_stdm(int busy, int data, int addr, MuxUse *out);
/* 1：STDM 更高。-1：同步 TDM 更高。0：一样，或这一帧没有数据。 */
int mux_cmp(const MuxUse *tdm, const MuxUse *stdm);

#endif
