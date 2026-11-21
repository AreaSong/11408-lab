#include "net/net_mip.h"

#include "bytes.h"
#include "net/net_ip.h"

#include <string.h>

static unsigned outer_sum(uint32_t dst, int ttl) {
    unsigned char hdr[20];
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x45;
    write_be16(hdr + 2, 62);
    write_be16(hdr + 4, 2);
    write_be16(hdr + 6, 0x4000);
    hdr[9] = 4;
    write_be32(hdr + 12, 0x0a000001u);
    write_be32(hdr + 16, dst);
    net_ip_set(hdr, ttl, 0, 0);
    return read_be16(hdr + 10);
}

int net_mip_tunnel(unsigned *cksum, unsigned *after, unsigned *colo, unsigned *colo_after) {
    if (cksum == NULL || after == NULL || colo == NULL || colo_after == NULL) {
        return -1;
    }
    *cksum = outer_sum(0x1e000001u, 64);
    *after = outer_sum(0x1e000001u, 63);
    *colo = outer_sum(0x1e000032u, 64);
    *colo_after = outer_sum(0x1e000032u, 63);
    return 0;
}
