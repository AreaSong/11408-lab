#ifndef SOCK_H
#define SOCK_H

#include <stddef.h>

int sock_port(const char *s, unsigned short *port);
int listen_loopback(unsigned short port);
int connect_loopback(unsigned short port);
int write_all(int fd, const void *buf, size_t n);

#endif
