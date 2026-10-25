#ifndef ROUTE_H
#define ROUTE_H

#include <stdbool.h>
#include <stdio.h>

#define ROUTE_NAME 16
#define ROUTE_MAX_LINKS 32
#define ROUTE_MAX_PKTS 16
#define ROUTE_MAX_HOPS 8

typedef struct {
    char a[ROUTE_NAME];
    char b[ROUTE_NAME];
    long long ns;
} RouteLink;

typedef struct {
    int nodes;
    char name[ROUTE_MAX_HOPS][ROUTE_NAME];
} RoutePath;

typedef struct {
    int id;
    RoutePath path;
} RoutePacket;

typedef struct {
    long long interval_ns;
    long long setup_ns;
    int has_interval;
    int has_setup;
    int has_vc;
    int nlinks;
    int npackets;
    RouteLink links[ROUTE_MAX_LINKS];
    RoutePath vc;
    RoutePacket packets[ROUTE_MAX_PKTS];
} RouteScene;

/* 无向链路。路径时延是沿途各段时延之和 */
bool route_path_ns(const RouteLink *links, int nlinks, const RoutePath *path, long long *ns);

/* arrivals[i] = setup + i × interval + path_ns[i]。乘加溢出时返回 false */
bool route_arrivals(long long setup, long long interval, const long long *path_ns, int n,
                    long long *arrivals);

/* 到达时刻早的在前；同时到达时保持发送顺序 */
void route_order(const long long *arrivals, int n, int *order);

int route_report(FILE *out, const RouteScene *scene);

#endif
