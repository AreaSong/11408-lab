#include "wifi11.h"

#include <string.h>

static void put3(WifiFrame *w, WifiScene scene, int to_ds, int from_ds, const unsigned char a1[MAC_LEN],
                 const unsigned char a2[MAC_LEN], const unsigned char a3[MAC_LEN]) {
    memset(w, 0, sizeof *w);
    w->scene = scene;
    w->to_ds = to_ds;
    w->from_ds = from_ds;
    memcpy(w->addr[0], a1, MAC_LEN);
    memcpy(w->addr[1], a2, MAC_LEN);
    memcpy(w->addr[2], a3, MAC_LEN);
    w->naddr = 3;
}

/* 地址 1、2 永远是这一跳的接收方和发送方。地址 3 补上被挤掉的那一端。 */
void wifi_sta_ap(WifiFrame *w, const unsigned char sta[MAC_LEN], const unsigned char ap[MAC_LEN],
                 const unsigned char dest[MAC_LEN]) {
    put3(w, WIFI_STA_AP, 1, 0, ap, sta, dest);
}

void wifi_ap_sta(WifiFrame *w, const unsigned char ap[MAC_LEN], const unsigned char dest[MAC_LEN],
                 const unsigned char src[MAC_LEN]) {
    put3(w, WIFI_AP_STA, 0, 1, dest, ap, src);
}

void wifi_adhoc(WifiFrame *w, const unsigned char src[MAC_LEN], const unsigned char dest[MAC_LEN],
                const unsigned char bssid[MAC_LEN]) {
    put3(w, WIFI_ADHOC, 0, 0, dest, src, bssid);
}

void wifi_wds(WifiFrame *w, const unsigned char ra[MAC_LEN], const unsigned char ta[MAC_LEN],
              const unsigned char dest[MAC_LEN], const unsigned char src[MAC_LEN]) {
    put3(w, WIFI_WDS, 1, 1, ra, ta, dest);
    memcpy(w->addr[3], src, MAC_LEN);
    w->naddr = 4;
}
