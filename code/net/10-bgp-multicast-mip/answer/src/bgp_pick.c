#include "bgp.h"
#include "bgp_int.h"

#include <stdio.h>
#include <string.h>

static BgpSpeaker *speaker_of(BgpNet *net, const char *name) {
    int i;

    for (i = 0; i < net->nspeaker; i++) {
        if (strcmp(net->speaker[i].name, name) == 0) {
            return &net->speaker[i];
        }
    }
    return NULL;
}

static const BgpSpeaker *speaker_of_c(const BgpNet *net, const char *name) {
    int i;

    for (i = 0; i < net->nspeaker; i++) {
        if (strcmp(net->speaker[i].name, name) == 0) {
            return &net->speaker[i];
        }
    }
    return NULL;
}

int bgp_add_offer(BgpNet *net, const char *speaker, const char *prefix, const char *via, int igp,
                  const int *path, int npath, char *err, int err_cap) {
    BgpSpeaker *sp;
    BgpOffer *offer;
    int i;

    if (!bgp_name_ok(speaker) || !bgp_name_ok(via)) {
        snprintf(err, (size_t)err_cap, "路由器名字用字母开头，最长 7 位");
        return 0;
    }
    if (igp < 0 || igp > 255 || npath < 1 || npath > BGP_PATH_MAX) {
        snprintf(err, (size_t)err_cap, "IGP 距离或 AS 路径不合法");
        return 0;
    }
    sp = speaker_of(net, speaker);
    if (sp == NULL) {
        if (net->nspeaker >= BGP_SPEAKER_MAX) {
            snprintf(err, (size_t)err_cap, "路由器太多");
            return 0;
        }
        sp = &net->speaker[net->nspeaker++];
        memset(sp, 0, sizeof *sp);
        snprintf(sp->name, sizeof sp->name, "%s", speaker);
    }
    for (i = 0; i < sp->noffer; i++) {
        if (strcmp(sp->offer[i].prefix, prefix) == 0 && strcmp(sp->offer[i].via, via) == 0) {
            snprintf(err, (size_t)err_cap, "下一跳 %s 已经通告过 %s", via, prefix);
            return 0;
        }
    }
    if (sp->noffer >= BGP_OFFER_MAX) {
        snprintf(err, (size_t)err_cap, "候选路由太多");
        return 0;
    }
    offer = &sp->offer[sp->noffer++];
    memset(offer, 0, sizeof *offer);
    snprintf(offer->via, sizeof offer->via, "%s", via);
    snprintf(offer->prefix, sizeof offer->prefix, "%s", prefix);
    offer->npath = npath;
    offer->igp = igp;
    memcpy(offer->path, path, (size_t)npath * sizeof(int));
    return 1;
}

static int matching_offers(const BgpSpeaker *sp, const char *prefix, int *idx, int cap) {
    int i;
    int n = 0;

    for (i = 0; i < sp->noffer; i++) {
        if (strcmp(sp->offer[i].prefix, prefix) != 0) {
            continue;
        }
        if (n >= cap) {
            return -1;
        }
        idx[n++] = i;
    }
    return n;
}

static int choose_offer(const BgpSpeaker *sp, const int *idx, int n) {
    int best = 0;
    int i;

    for (i = 1; i < n; i++) {
        const BgpOffer *a = &sp->offer[idx[i]];
        const BgpOffer *b = &sp->offer[idx[best]];
        if (a->npath < b->npath || (a->npath == b->npath && a->igp < b->igp)) {
            best = i;
        }
    }
    return best;
}

static void print_candidate(FILE *fp, const BgpOffer *offer) {
    char shown[80];

    path_text(offer->path, offer->npath, shown, (int)sizeof shown);
    say(fp, "候选 下一跳 %s，AS 路径 %s，AS 数 %d 个，本 AS 内 IGP 距离 %d 跳\n", offer->via, shown,
        offer->npath, offer->igp);
}

static void explain_choice(FILE *fp, const BgpSpeaker *sp, const int *idx, int n, int best) {
    const BgpOffer *win = &sp->offer[idx[best]];
    int i;
    int tied = 0;

    say(fp, "先比 AS 路径里有几个自治系统。本地优先级本题没有给，看成一样。\n");
    for (i = 0; i < n; i++) {
        const BgpOffer *offer = &sp->offer[idx[i]];
        if (offer->npath > win->npath) {
            say(fp, "下一跳 %s 有 %d 个 AS，比 %d 个长，去掉。\n", offer->via, offer->npath, win->npath);
        }
    }
    say(fp, "剩下的路径都是 %d 个 AS。再比本 AS 内到出口的 IGP 距离，让分组尽快离开本 AS。\n",
        win->npath);
    for (i = 0; i < n; i++) {
        const BgpOffer *offer = &sp->offer[idx[i]];
        if (offer->npath == win->npath && offer->igp > win->igp) {
            say(fp, "下一跳 %s 的 IGP 距离是 %d 跳，比 %d 跳大，去掉。\n", offer->via, offer->igp,
                win->igp);
        }
        if (offer->npath == win->npath && offer->igp == win->igp) {
            tied++;
        }
    }
    if (tied > 1) {
        say(fp, "有 %d 条一样短，保留先收到的 %s。\n", tied, win->via);
    }
    say(fp, "下一跳 %s 的 AS 数是 %d 个，IGP 距离是 %d 跳。\n", win->via, win->npath, win->igp);
}

int bgp_pick(const BgpNet *net, const char *speaker, const char *prefix, char *via, int via_cap,
             FILE *fp, char *err, int err_cap) {
    const BgpSpeaker *sp = speaker_of_c(net, speaker);
    int idx[BGP_OFFER_MAX];
    int n;
    int best;
    int i;

    if (sp == NULL) {
        snprintf(err, (size_t)err_cap, "还没有 %s 的候选路由", speaker);
        return 0;
    }
    n = matching_offers(sp, prefix, idx, BGP_OFFER_MAX);
    if (n <= 0) {
        snprintf(err, (size_t)err_cap, "%s 没有到 %s 的候选", speaker, prefix);
        return 0;
    }
    best = choose_offer(sp, idx, n);
    say(fp, "【选路】%s 到 %s\n", speaker, prefix);
    for (i = 0; i < n; i++) {
        print_candidate(fp, &sp->offer[idx[i]]);
    }
    explain_choice(fp, sp, idx, n, best);
    snprintf(via, (size_t)via_cap, "%s", sp->offer[idx[best]].via);
    say(fp, "结果：下一跳 %s\n", via);
    return 1;
}
