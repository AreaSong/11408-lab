#ifndef TOPO_H
#define TOPO_H

#include "mactab.h"

#define TOPO_DEV 24
#define TOPO_LINK 32
#define DEV_DEG 8
#define DOM_MEM 16
#define DOM_MAX 16
#define NAME_LEN 16

typedef enum { DEV_HOST = 1, DEV_HUB, DEV_SWITCH, DEV_ROUTER } DevKind;

typedef struct {
    char name[NAME_LEN];
    char ip[NAME_LEN];
    DevKind kind;
    unsigned char mac[MAC_LEN];
    int nlink;
    int link[DEV_DEG];
    int pvlan[DEV_DEG];
    MacTab tab;
} Device;

typedef struct {
    int a;
    int b;
} Link;

typedef struct {
    Device dev[TOPO_DEV];
    int ndev;
    Link link[TOPO_LINK];
    int nlink;
} Topo;

typedef struct {
    int mem[DOM_MEM];
    int n;
} Group;

typedef struct {
    Group col[DOM_MAX];
    int ncol;
    Group brd[DOM_MAX];
    int nbrd;
    int col_of[TOPO_DEV];
    int brd_of[TOPO_DEV];
} DomainReport;

void topo_init(Topo *t);
int topo_add(Topo *t, DevKind kind, const char *name, char *err);
int topo_add_host(Topo *t, const char *name, const unsigned char mac[MAC_LEN], const char *ip,
                  char *err);
int topo_find(const Topo *t, const char *name);
/* vlan < 0 表示交换机端口用默认 VLAN 1。 */
int topo_link(Topo *t, const char *a, const char *b, int vlan, char *err);
int topo_domains(Topo *t, DomainReport *r, char *err);
int topo_star(Topo *t, DevKind kind, int n, char *err);

#endif
