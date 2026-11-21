#include "user/user_udp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int bind_local(int fd, struct sockaddr_in *addr) {
    socklen_t len = sizeof *addr;
    memset(addr, 0, sizeof *addr);
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(0x7f000001);
    addr->sin_port = 0;
    if (bind(fd, (struct sockaddr *)addr, sizeof *addr) != 0) {
        return -1;
    }
    return getsockname(fd, (struct sockaddr *)addr, &len) == 0 ? 0 : -1;
}

int user_udp_once(char *out, int cap) {
    int left = socket(AF_INET, SOCK_DGRAM, 0);
    int right = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    struct timeval tv;
    char buf[8];
    ssize_t nread;
    int n;
    if (left < 0 || right < 0) {
        if (left >= 0) {
            close(left);
        }
        if (right >= 0) {
            close(right);
        }
        return -1;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    if (bind_local(left, &addr) != 0 || setsockopt(left, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) != 0 ||
        sendto(right, "MM", 2, 0, (struct sockaddr *)&addr, sizeof addr) != 2) {
        close(left);
        close(right);
        return -1;
    }
    nread = recvfrom(left, buf, sizeof buf, 0, NULL, NULL);
    close(left);
    close(right);
    if (nread != 2 || memcmp(buf, "MM", 2) != 0) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "udp bytes=2 text=MM");
    return n > 0 && n < cap ? 0 : -1;
}
