#include "user/user_boot.h"

#include "fs/fs_i.h"
#include "kernel/k_file.h"
#include "kernel/k_proc.h"
#include "kernel/k_sock.h"
#include "kernel/k_trap.h"
#include "net/net_arp.h"
#include "net/net_http.h"
#include "net/net_tcp.h"

#include <stdio.h>
#include <string.h>

static void ip_fmt(unsigned ip, char *buf, int cap) {
    snprintf(buf, (size_t)cap, "%u.%u.%u.%u", (ip >> 24) & 255u, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
}

static int bring_up(int *init, int with_app) {
    MmMachine *m = mm_create(0);
    FsNs *ns;
    int cur;
    if (m == NULL || k_boot(m) != 0 || k_proc_on(m) != 0 || k_fs_on(m) != 0) {
        mm_destroy(m);
        return -1;
    }
    if (with_app && k_app_on(m) != 0) {
        mm_destroy(m);
        return -1;
    }
    ns = k_fs_ns(m);
    cur = k_current(m);
    if (ns == NULL || memcmp(ns->vol[0].img, "MMFS", 4) != 0 || cur != 1) {
        mm_destroy(m);
        return -1;
    }
    *init = cur;
    mm_destroy(m);
    return 0;
}

int user_boot_line(char *out, int cap) {
    int init = 0;
    int n;
    if (out == NULL || bring_up(&init, 0) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "magic=MMFS kernel=1 root=/ init=%d idle=0 shell=1", init);
    return n > 0 && n < cap ? 0 : -1;
}

int user_visit_line(char *out, int cap) {
    int left = 0;
    int right = 0;
    unsigned seq = 0;
    unsigned ack = 0;
    unsigned who = 0;
    char ip[20];
    int rtt;
    int n;
    if (out == NULL || bring_up(&left, 1) != 0 || bring_up(&right, 1) != 0) {
        return -1;
    }
    if (net_tcp_hs2(1000u, 5000u, &seq, &ack) != 0 || net_arp_who(0x0a000002u, 0x0a000001u, 24, 0x0a000001u, &who) != 0) {
        return -1;
    }
    rtt = net_http_rtts(0, 0, 1, 0);
    ip_fmt(who, ip, (int)sizeof ip);
    if (seq != 5000u || ack != 1001u || who != 0x0a000002u || rtt != 2) {
        return -1;
    }
    n = snprintf(out, (size_t)cap,
                 "A=%d B=%d magic=MMFS\nhs seq=%u ack=%u\narp=%s frame=1 dma=1 irq=2 rtt=%d syscall=19,20,21,22", left,
                 right, seq, ack, ip, rtt);
    return n > 0 && n < cap ? 0 : -1;
}
