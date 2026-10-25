#include "mactab.h"

#include <string.h>

void mactab_init(MacTab *t) {
    memset(t, 0, sizeof *t);
}

/* 转发表每个帧都要查。键是 MAC 再加 VLAN：同一个地址在另一个 VLAN 里是另一台主机。 */
static unsigned mac_hash(const unsigned char mac[MAC_LEN], int vlan) {
    unsigned h = 2166136261u;
    int i;

    for (i = 0; i < MAC_LEN; i++) {
        h ^= mac[i];
        h *= 16777619u;
    }
    h ^= (unsigned)vlan;
    h *= 16777619u;
    return h;
}

static int same_key(const MacSlot *s, const unsigned char mac[MAC_LEN], int vlan) {
    return s->used && s->vlan == vlan && mac_eq(s->mac, mac);
}

int mactab_learn(MacTab *t, const unsigned char mac[MAC_LEN], int vlan, int port,
                 int *old_port) {
    unsigned start = mac_hash(mac, vlan) % MACTAB_CAP;
    unsigned i;
    int blank = -1;

    for (i = 0; i < MACTAB_CAP; i++) {
        unsigned k = (start + i) % MACTAB_CAP;
        MacSlot *s = &t->slot[k];

        if (!s->used) {
            blank = (int)k;
            break;
        }
        if (same_key(s, mac, vlan)) {
            if (s->port == port) {
                return 2;
            }
            if (old_port != NULL) {
                *old_port = s->port;
            }
            s->port = port;
            return 3;
        }
    }
    if (blank < 0) {
        return 0;
    }
    memcpy(t->slot[blank].mac, mac, MAC_LEN);
    t->slot[blank].vlan = vlan;
    t->slot[blank].port = port;
    t->slot[blank].used = 1;
    t->count++;
    return 1;
}

int mactab_find(const MacTab *t, const unsigned char mac[MAC_LEN], int vlan, int *port) {
    unsigned start = mac_hash(mac, vlan) % MACTAB_CAP;
    unsigned i;

    for (i = 0; i < MACTAB_CAP; i++) {
        unsigned k = (start + i) % MACTAB_CAP;
        const MacSlot *s = &t->slot[k];

        if (!s->used) {
            return 0;
        }
        if (same_key(s, mac, vlan)) {
            if (port != NULL) {
                *port = s->port;
            }
            return 1;
        }
    }
    return 0;
}

static int row_before(const MacRow *a, const MacRow *b) {
    int c;

    if (a->vlan != b->vlan) {
        return a->vlan < b->vlan;
    }
    if (a->port != b->port) {
        return a->port < b->port;
    }
    c = memcmp(a->mac, b->mac, MAC_LEN);
    return c < 0;
}

int mactab_rows(const MacTab *t, MacRow *rows, int cap) {
    int n = 0;
    int i;
    int j;

    for (i = 0; i < MACTAB_CAP; i++) {
        if (!t->slot[i].used || n >= cap) {
            continue;
        }
        memcpy(rows[n].mac, t->slot[i].mac, MAC_LEN);
        rows[n].vlan = t->slot[i].vlan;
        rows[n].port = t->slot[i].port;
        n++;
    }
    for (i = 1; i < n; i++) {
        MacRow key = rows[i];
        j = i;
        while (j > 0 && row_before(&key, &rows[j - 1])) {
            rows[j] = rows[j - 1];
            j--;
        }
        rows[j] = key;
    }
    return n;
}
