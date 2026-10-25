#include "topo.h"

#include "scan.h"

#include <stdio.h>
#include <string.h>

void topo_init(Topo *t) {
    memset(t, 0, sizeof *t);
}

int topo_find(const Topo *t, const char *name) {
    int i;

    for (i = 0; i < t->ndev; i++) {
        if (strcmp(t->dev[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int good_name(const char *name) {
    int i;

    if (name == NULL || name[0] == '\0' || !isalpha((unsigned char)name[0])) {
        return 0;
    }
    for (i = 0; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) || i >= NAME_LEN - 1) {
            return 0;
        }
    }
    return 1;
}

static int add_dev(Topo *t, DevKind kind, const char *name, char *err) {
    Device *d;

    if (!good_name(name)) {
        return lab_fail(err, "名字以字母开头，只用字母和数字，最长 15 个字符");
    }
    if (topo_find(t, name) >= 0) {
        return lab_fail(err, "名字已经用过了");
    }
    if (t->ndev >= TOPO_DEV) {
        return lab_fail(err, "设备太多了");
    }
    d = &t->dev[t->ndev];
    memset(d, 0, sizeof *d);
    snprintf(d->name, NAME_LEN, "%s", name);
    d->kind = kind;
    mactab_init(&d->tab);
    t->ndev++;
    return 1;
}

int topo_add(Topo *t, DevKind kind, const char *name, char *err) {
    if (kind == DEV_HOST) {
        return lab_fail(err, "主机用 host 命令");
    }
    return add_dev(t, kind, name, err);
}

int topo_add_host(Topo *t, const char *name, const unsigned char mac[MAC_LEN], const char *ip,
                  char *err) {
    int i;

    if (!add_dev(t, DEV_HOST, name, err)) {
        return 0;
    }
    for (i = 0; i < t->ndev - 1; i++) {
        if (t->dev[i].kind == DEV_HOST && mac_eq(t->dev[i].mac, mac)) {
            t->ndev--;
            return lab_fail(err, "MAC 地址已经有主机在用");
        }
    }
    memcpy(t->dev[t->ndev - 1].mac, mac, MAC_LEN);
    if (ip != NULL && ip[0] != '\0') {
        if (strlen(ip) >= NAME_LEN) {
            t->ndev--;
            return lab_fail(err, "IP 地址太长");
        }
        snprintf(t->dev[t->ndev - 1].ip, NAME_LEN, "%s", ip);
    }
    return 1;
}

static int attach(Device *d, int link, int vlan) {
    if (d->nlink >= DEV_DEG) {
        return 0;
    }
    d->link[d->nlink] = link;
    d->pvlan[d->nlink] = vlan;
    d->nlink++;
    return 1;
}

static int link_vlan(const Device *a, const Device *b, int vlan, int *out, char *err) {
    int has_sw = a->kind == DEV_SWITCH || b->kind == DEV_SWITCH;

    if (vlan >= 0 && !has_sw) {
        return lab_fail(err, "只有交换机的端口才指定 VLAN");
    }
    if (vlan >= 0 && (vlan < 1 || vlan > 4094)) {
        return lab_fail(err, "VLAN 号用 1～4094，0 和 4095 保留");
    }
    *out = vlan >= 0 ? vlan : 1;
    return 1;
}

int topo_link(Topo *t, const char *a, const char *b, int vlan, char *err) {
    int ia = topo_find(t, a);
    int ib = topo_find(t, b);
    int use = 1;
    int id;

    if (ia < 0 || ib < 0) {
        return lab_fail(err, "链路两端都要先建好");
    }
    if (ia == ib) {
        return lab_fail(err, "不能自己连自己");
    }
    if (t->dev[ia].kind == DEV_HOST && t->dev[ib].kind == DEV_HOST) {
        return lab_fail(err, "两台主机要经过集线器、交换机或路由器");
    }
    if ((t->dev[ia].kind == DEV_HOST && t->dev[ia].nlink > 0) ||
        (t->dev[ib].kind == DEV_HOST && t->dev[ib].nlink > 0)) {
        return lab_fail(err, "一台主机只接一条链路");
    }
    if (!link_vlan(&t->dev[ia], &t->dev[ib], vlan, &use, err)) {
        return 0;
    }
    if (t->nlink >= TOPO_LINK) {
        return lab_fail(err, "链路太多了");
    }
    id = t->nlink;
    if (!attach(&t->dev[ia], id, t->dev[ia].kind == DEV_SWITCH ? use : 0)) {
        return lab_fail(err, "端口接满了");
    }
    if (!attach(&t->dev[ib], id, t->dev[ib].kind == DEV_SWITCH ? use : 0)) {
        t->dev[ia].nlink--;
        return lab_fail(err, "端口接满了");
    }
    t->link[id].a = ia;
    t->link[id].b = ib;
    t->nlink++;
    return 1;
}
