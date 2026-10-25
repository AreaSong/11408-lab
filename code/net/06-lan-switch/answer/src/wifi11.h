#ifndef WIFI11_H
#define WIFI11_H

#include "macaddr.h"

typedef enum { WIFI_STA_AP = 1, WIFI_AP_STA, WIFI_ADHOC, WIFI_WDS } WifiScene;

typedef struct {
    WifiScene scene;
    int to_ds;
    int from_ds;
    unsigned char addr[4][MAC_LEN];
    int naddr;
} WifiFrame;

void wifi_sta_ap(WifiFrame *w, const unsigned char sta[MAC_LEN], const unsigned char ap[MAC_LEN],
                 const unsigned char dest[MAC_LEN]);
void wifi_ap_sta(WifiFrame *w, const unsigned char ap[MAC_LEN], const unsigned char dest[MAC_LEN],
                 const unsigned char src[MAC_LEN]);
void wifi_adhoc(WifiFrame *w, const unsigned char src[MAC_LEN], const unsigned char dest[MAC_LEN],
                const unsigned char bssid[MAC_LEN]);
void wifi_wds(WifiFrame *w, const unsigned char ra[MAC_LEN], const unsigned char ta[MAC_LEN],
              const unsigned char dest[MAC_LEN], const unsigned char src[MAC_LEN]);

#endif
