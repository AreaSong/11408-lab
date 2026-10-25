#include "fwdlay.h"

#include "qty.h"
#include "scan.h"

int fwdlay_ns(int store, long long bps, long long frame_bytes, LayTime *out, char *err) {
    long long nbit = 0;
    long long ns = 0;
    double bytes;

    if (bps <= 0) {
        return lab_fail(err, "带宽要大于 0");
    }
    bytes = store ? (double)frame_bytes : 6.0;
    if (store && (frame_bytes <= 0 || frame_bytes > 10000)) {
        return lab_fail(err, "帧长用正整数，单位是字节");
    }
    if (!parse_bits(bytes, "B", &nbit)) {
        return lab_fail(err, "帧长换不成整数个比特");
    }
    if (!qty_div_ns(nbit, bps, &ns)) {
        return lab_fail(err, "发送时间除不尽，不四舍五入");
    }
    out->bits = nbit;
    out->ns = ns;
    return 1;
}
