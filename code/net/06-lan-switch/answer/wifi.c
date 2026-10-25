#include "scan.h"
#include "show_wifi.h"

#include <stdio.h>
#include <string.h>

static int die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int take(const char **p, char *buf, int cap) {
    int w = scan_word(p, buf, cap);

    if (w < 0) {
        die("这个词太长");
        return 0;
    }
    if (w == 0) {
        die("命令不完整");
        return 0;
    }
    return 1;
}

static int take_key(const char **p, const char *key) {
    char w[16];

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (strcmp(w, key) != 0) {
        fprintf(stderr, "这里应该是 %s\n", key);
        return 0;
    }
    return 1;
}

static int take_mac(const char **p, unsigned char mac[MAC_LEN]) {
    char w[24];
    char err[LAB_ERR];

    if (!take(p, w, (int)sizeof w)) {
        return 0;
    }
    if (!mac_parse(w, mac, err)) {
        die(err);
        return 0;
    }
    return 1;
}

static int extra(const char *p) {
    if (scan_blank(p)) {
        return 0;
    }
    return die("多了无法识别的内容");
}

static int cmd_sta_ap(const char *p, WifiFrame *frame) {
    unsigned char sta[MAC_LEN];
    unsigned char ap[MAC_LEN];
    unsigned char dest[MAC_LEN];

    if (!take_key(&p, "sta") || !take_mac(&p, sta) || !take_key(&p, "ap") || !take_mac(&p, ap) ||
        !take_key(&p, "dest") || !take_mac(&p, dest) || extra(p)) {
        return 1;
    }
    wifi_sta_ap(frame, sta, ap, dest);
    return 0;
}

static int cmd_ap_sta(const char *p, WifiFrame *frame) {
    unsigned char ap[MAC_LEN];
    unsigned char dest[MAC_LEN];
    unsigned char src[MAC_LEN];

    if (!take_key(&p, "ap") || !take_mac(&p, ap) || !take_key(&p, "dest") || !take_mac(&p, dest) ||
        !take_key(&p, "src") || !take_mac(&p, src) || extra(p)) {
        return 1;
    }
    wifi_ap_sta(frame, ap, dest, src);
    return 0;
}

static int cmd_adhoc(const char *p, WifiFrame *frame) {
    unsigned char src[MAC_LEN];
    unsigned char dest[MAC_LEN];
    unsigned char bssid[MAC_LEN];

    if (!take_key(&p, "src") || !take_mac(&p, src) || !take_key(&p, "dest") || !take_mac(&p, dest) ||
        !take_key(&p, "bssid") || !take_mac(&p, bssid) || extra(p)) {
        return 1;
    }
    wifi_adhoc(frame, src, dest, bssid);
    return 0;
}

static int cmd_wds(const char *p, WifiFrame *frame) {
    unsigned char ra[MAC_LEN];
    unsigned char ta[MAC_LEN];
    unsigned char dest[MAC_LEN];
    unsigned char src[MAC_LEN];

    if (!take_key(&p, "ra") || !take_mac(&p, ra) || !take_key(&p, "ta") || !take_mac(&p, ta) ||
        !take_key(&p, "dest") || !take_mac(&p, dest) || !take_key(&p, "src") || !take_mac(&p, src) ||
        extra(p)) {
        return 1;
    }
    wifi_wds(frame, ra, ta, dest, src);
    return 0;
}

static int wifi_command(const char *line) {
    char cmd[16];
    const char *p = line;
    WifiFrame frame;
    int w = scan_word(&p, cmd, (int)sizeof cmd);
    int rc;

    if (w < 0) {
        return die("这个词太长");
    }
    if (strcmp(cmd, "sta-ap") == 0) {
        rc = cmd_sta_ap(p, &frame);
    } else if (strcmp(cmd, "ap-sta") == 0) {
        rc = cmd_ap_sta(p, &frame);
    } else if (strcmp(cmd, "adhoc") == 0) {
        rc = cmd_adhoc(p, &frame);
    } else if (strcmp(cmd, "wds") == 0) {
        rc = cmd_wds(p, &frame);
    } else {
        return die("不认识的命令");
    }
    if (rc != 0) {
        return 1;
    }
    show_wifi(&frame);
    return 0;
}

int main(void) {
    return lab_lines("请输入命令（sta-ap / ap-sta / adhoc / wds），Ctrl+D 结束：", wifi_command);
}
