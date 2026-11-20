#ifndef SDN_H
#define SDN_H

#include "topo.h"

/*
 * 控制器掌握拓扑，算出每台交换机的匹配-动作。
 * 数据平面只查已经下发的流表，链路断开后不会自己重算。
 */

#define SDN_HMAX 16
#define SDN_DELIVER -1
#define SDN_DROP -2

typedef struct {
    Topo topo;
    int nh;
    char hname[SDN_HMAX][TOPO_NAME];
    int home[SDN_HMAX];
    int port[G_NMAX][G_NMAX];
    int nport[G_NMAX];
    int flow[G_NMAX][SDN_HMAX];
    int ready;
} Sdn;

void sdn_init(Sdn *s);
int sdn_find_host(const Sdn *s, const char *name);
int sdn_add_switch(Sdn *s, const char *name, char *err, int cap);
int sdn_add_host(Sdn *s, const char *host, const char *sw, char *err, int cap);
int sdn_link(Sdn *s, const char *a, const char *b, int cost, char *err, int cap);
int sdn_install(Sdn *s, char *err, int cap);
int sdn_fail(Sdn *s, const char *a, const char *b, char *err, int cap);
int sdn_next(const Sdn *s, int sw, int host);

#endif
