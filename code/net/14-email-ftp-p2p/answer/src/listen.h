#ifndef LISTEN_H
#define LISTEN_H

#include <stddef.h>

int listen_loopback(unsigned short port);
int write_all(int fd, const void *buf, size_t n);

#endif
