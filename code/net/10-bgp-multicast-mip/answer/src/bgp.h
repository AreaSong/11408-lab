#ifndef BGP_H
#define BGP_H

#include <stdio.h>

#define BGP_AS_MAX 16
#define BGP_PEER_MAX 32
#define BGP_ROUTE_MAX 48
#define BGP_PATH_MAX 12
#define BGP_PREFIX_MAX 20
#define BGP_NAME_MAX 8
#define BGP_OFFER_MAX 8
#define BGP_SPEAKER_MAX 8

typedef struct {
    int number;
} BgpAs;

typedef struct {
    int a;
    int b;
} BgpPeer;

typedef struct {
    char prefix[BGP_PREFIX_MAX];
    int holder;
    int path[BGP_PATH_MAX];
    int npath;
    int local;
} BgpRoute;

typedef struct {
    char via[BGP_NAME_MAX];
    char prefix[BGP_PREFIX_MAX];
    int path[BGP_PATH_MAX];
    int npath;
    int igp;
} BgpOffer;

typedef struct {
    char name[BGP_NAME_MAX];
    BgpOffer offer[BGP_OFFER_MAX];
    int noffer;
} BgpSpeaker;

typedef struct {
    BgpAs as[BGP_AS_MAX];
    int nas;
    BgpPeer peer[BGP_PEER_MAX];
    int npeer;
    BgpRoute route[BGP_ROUTE_MAX];
    int nroute;
    BgpSpeaker speaker[BGP_SPEAKER_MAX];
    int nspeaker;
    int round;
} BgpNet;

void bgp_init(BgpNet *net);
int bgp_parse_int(const char *text, int min, int max, int *out, const char *what, char *err,
                  int err_cap);
int bgp_prefix_ok(const char *text, char *out, int out_cap, char *err, int err_cap);
int bgp_name_ok(const char *text);
int bgp_has_as(const BgpNet *net, int number);
int bgp_add_as(BgpNet *net, int number, char *err, int err_cap);
int bgp_add_peer(BgpNet *net, int a, int b, char *err, int err_cap);
int bgp_add_origin(BgpNet *net, int as, const char *prefix, char *err, int err_cap);
int bgp_step(BgpNet *net, FILE *fp);
int bgp_path(const BgpNet *net, int holder, const char *prefix, int *path, int cap, int *local);
int bgp_add_offer(BgpNet *net, const char *speaker, const char *prefix, const char *via, int igp,
                  const int *path, int npath, char *err, int err_cap);
int bgp_pick(const BgpNet *net, const char *speaker, const char *prefix, char *via, int via_cap,
             FILE *fp, char *err, int err_cap);

#endif
