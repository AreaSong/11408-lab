#ifndef NET_SW_H
#define NET_SW_H

typedef struct {
    char mac[8][32];
    int port[8];
    int vlan[8];
    int n;
} NetTable;

int net_learn(NetTable *t, int port, const char *mac, int vlan);
int net_forward(const NetTable *t, int in_port, const char *dst, int vlan, int nports, int *out, int cap);
int net_sw_2014(char *first, int c1, char *second, int c2);
int net_sw_2016(char *first, int c1, char *second, int c2);
int net_domains(int hubs, int routers, int *collision, int *broadcast);
int net_place(const char *dev, int hosts, int *collision, int *broadcast);
int net_fit_2022(char *dev1, int c1, char *dev2, int c2);
int net_vlan_2024(char *miss, int *ports, int cap, int *nports);
int net_wifi_exam(const char *id, char *dst, int cap);
long long net_cut_ns(long long bytes, long long mbps);
int net_lan_line(char *dst, int cap);

#endif
