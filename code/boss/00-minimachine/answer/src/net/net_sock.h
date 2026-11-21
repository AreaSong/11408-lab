#ifndef NET_SOCK_H
#define NET_SOCK_H

#include <stdint.h>

typedef struct AppNs AppNs;

AppNs *net_app_new(void);
void net_app_free(AppNs *a);
void net_app_clear(AppNs *a);
int net_app_socket(AppNs *a, int type, int port);
int net_app_connect(AppNs *a, int id, uint32_t ip, int port);
int net_app_send(AppNs *a, int id, const void *buf, int n);
int net_app_recv(AppNs *a, int id, void *buf, int cap);
int net_session_line(char *out, int cap);

#endif
