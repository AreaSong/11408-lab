#ifndef K_SOCK_H
#define K_SOCK_H

#include "common/mm_machine.h"

#include <stdint.h>

/* 调用号一经使用不再改。没执行 app on 时，这四个号仍走「未知调用」。 */
enum {
    K_SYS_SOCKET = 19,
    K_SYS_SENDTO = 20,
    K_SYS_RECVFROM = 21,
    K_SYS_CONNECT = 22
};

int k_app_on(MmMachine *m);
void k_app_off(MmMachine *m);
void k_app_clear(MmMachine *m);
int k_sock_syscall(MmMachine *m, uint32_t num, uint32_t a, uint32_t b, uint32_t c);

#endif
