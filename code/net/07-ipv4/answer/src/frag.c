#include "frag.h"

#include "scan.h"

static int bad_sizes(int total, int header, int mtu, char *err, int err_cap) {
    if (header < 20 || header > 60 || header % 4 != 0) {
        return fail_msg(err, err_cap, "首部长度要是 20 到 60 之间的 4 的倍数");
    }
    if (total < header || total > 65535) {
        return fail_msg(err, err_cap, "总长度要盖住首部，并且不超过 65535 字节");
    }
    if (mtu < header + 1) {
        return fail_msg(err, err_cap, "MTU 至少要比首部多 1 字节");
    }
    return 1;
}

static int fill_pieces(FragPlan *out, int data, int header, int max_data, char *err, int err_cap) {
    int done = 0;
    int i = 0;

    while (done < data) {
        int chunk = data - done;

        if (i >= FRAG_MAX) {
            return fail_msg(err, err_cap, "分片太多");
        }
        if (chunk > max_data) {
            chunk = max_data;
            out->piece[i].mf = 1;
        } else {
            out->piece[i].mf = 0;
        }
        out->piece[i].data_len = chunk;
        out->piece[i].total_len = chunk + header;
        out->piece[i].offset_units = done / 8;
        done += chunk;
        i++;
    }
    out->count = i;
    return 1;
}

int frag_plan(int total, int header, int mtu, int df, FragPlan *out, char *err, int err_cap) {
    int data;
    int room;
    int max_data;

    if (!bad_sizes(total, header, mtu, err, err_cap)) {
        return 0;
    }
    data = total - header;
    room = mtu - header;
    /* 非最后一片的数据必须是 8 的倍数，所以先把 MTU 里能装的数据向下取整。 */
    max_data = (room / 8) * 8;
    out->data_len = data;
    out->max_data = max_data;
    out->count = 0;
    out->df_drop = 0;
    out->need = data > room;
    if (!out->need) {
        out->count = 1;
        out->piece[0].data_len = data;
        out->piece[0].total_len = total;
        out->piece[0].offset_units = 0;
        out->piece[0].mf = 0;
        return 1;
    }
    if (df) {
        out->df_drop = 1;
        return 1;
    }
    if (max_data < 8) {
        return fail_msg(err, err_cap, "去掉首部后凑不出 8 字节的片");
    }
    return fill_pieces(out, data, header, max_data, err, err_cap);
}
