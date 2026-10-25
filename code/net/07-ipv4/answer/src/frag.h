#ifndef FRAG_H
#define FRAG_H

#define FRAG_MAX 16

typedef struct {
    int data_len;
    int total_len;
    int offset_units;
    int mf;
} FragPiece;

typedef struct {
    int need;
    int df_drop;
    int max_data;
    int data_len;
    int count;
    FragPiece piece[FRAG_MAX];
} FragPlan;

/*
 * 按出口 MTU 把一个数据报切成尽可能大的片。
 * 除最后一片外，数据长度是 8 的倍数。DF=1 又放不下时 df_drop=1。
 */
int frag_plan(int total, int header, int mtu, int df, FragPlan *out, char *err, int err_cap);

#endif
