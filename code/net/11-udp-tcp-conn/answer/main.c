#include "run_udp.h"
#include "scan.h"

int main(void) {
    return command_loop("请输入命令（sum / build / recv / header / demux / eff），Ctrl+D 结束：",
                        run_udp) != 0;
}
