#include "time.h"

#include <stdio.h>

static int rev_ticks(int rpm, long long *out) {
    const long long minute = 180000000000LL;

    if (rpm <= 0 || minute % rpm != 0) {
        return -1;
    }
    *out = minute / rpm;
    if (*out % 2 != 0) {
        return -1;
    }
    return 0;
}

int time_each(long long seek_us, int rpm, int sectors, int count, long long ctrl_us,
              TimeParts *out) {
    long long rev;

    if (rev_ticks(rpm, &rev) != 0 || sectors <= 0 || count <= 0 || rev % sectors != 0) {
        return -1;
    }
    out->seek = seek_us * 3000 * count;
    out->rot = (rev / 2) * count;
    out->xfer = (rev / sectors) * count;
    out->ctrl = ctrl_us * 3000 * count;
    return 0;
}

int time_sum(long long seek_us, int rpm, int sectors, int count, long long ctrl_us,
             TimeParts *out) {
    long long rev;

    if (rev_ticks(rpm, &rev) != 0 || sectors <= 0 || count <= 0 || rev % sectors != 0) {
        return -1;
    }
    out->seek = seek_us * 3000;
    out->rot = (rev / 2) * count;
    out->xfer = (rev / sectors) * count;
    out->ctrl = ctrl_us * 3000 * count;
    return 0;
}

int time_rate(long long seek_us, int rpm, long long bytes, long long bps, long long ctrl_us,
              TimeParts *out) {
    long long rev;
    long long product;

    if (rev_ticks(rpm, &rev) != 0 || bytes < 0 || bps <= 0) {
        return -1;
    }
    product = bytes * 3000000000LL;
    if (bytes != 0 && product % bps != 0) {
        return -1;
    }
    out->seek = seek_us * 3000;
    out->rot = rev / 2;
    out->xfer = bytes == 0 ? 0 : product / bps;
    out->ctrl = ctrl_us * 3000;
    return 0;
}

int fmt_ms(long long ticks, int ndp, char *buf, int cap) {
    long long scale = 3000000;
    long long pow10 = 1;
    long long scaled;
    long long rem;
    int i;

    if (ndp < 1 || ndp > 3 || ticks < 0) {
        return -1;
    }
    for (i = 0; i < ndp; i++) {
        pow10 *= 10;
    }
    scaled = (ticks * pow10) / scale;
    rem = (ticks * pow10) % scale;
    if (rem * 2 >= scale) {
        scaled++;
    }
    snprintf(buf, (size_t)cap, "%lld.%0*lld", scaled / pow10, ndp, scaled % pow10);
    return 0;
}

void time_print(const TimeParts *parts) {
    char seek[32];
    char rot[32];
    char xfer[32];
    char ctrl[32];
    char total[32];
    char round1[32];
    long long sum = parts->seek + parts->rot + parts->xfer + parts->ctrl;

    fmt_ms(parts->seek, 3, seek, (int)sizeof seek);
    fmt_ms(parts->rot, 3, rot, (int)sizeof rot);
    fmt_ms(parts->xfer, 3, xfer, (int)sizeof xfer);
    fmt_ms(parts->ctrl, 3, ctrl, (int)sizeof ctrl);
    fmt_ms(sum, 3, total, (int)sizeof total);
    fmt_ms(sum, 1, round1, (int)sizeof round1);
    printf("seek-ms=%s rot-ms=%s xfer-ms=%s ctrl-ms=%s total-ms=%s round1=%s\n", seek, rot, xfer,
           ctrl, total, round1);
}

static int gcd_int(int a, int b) {
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int layout_run(int nsec, int gap, int nread, int proc, int *ticks, int *wait, int *proc_sum) {
    int step;
    int pos = 0;
    int i;
    int used = 0;
    int waited = 0;

    if (nsec < 1 || nsec > 64 || gap < 0 || gap >= nsec || nread < 1 || nread > nsec || proc < 0 ||
        proc > nsec) {
        return -1;
    }
    step = gap + 1;
    /* 整条磁道都要能排开。只保证本次 nread 不撞车，换个长度又会叠到同一扇区。 */
    if (gcd_int(step, nsec) != 1) {
        return -1;
    }
    for (i = 0; i < nread; i++) {
        int phys = (i * step) % nsec;
        int gap_ticks = (phys - pos + nsec) % nsec;

        waited += gap_ticks;
        used += gap_ticks + 1;
        pos = (phys + 1) % nsec;
        if (i + 1 < nread) {
            used += proc;
            pos = (pos + proc) % nsec;
        }
    }
    *ticks = used;
    *wait = waited;
    *proc_sum = proc * (nread - 1);
    return 0;
}
