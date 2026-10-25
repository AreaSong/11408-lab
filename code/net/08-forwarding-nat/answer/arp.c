#include "line.h"
#include "run_arp.h"

int main(void) {
    return command_loop("请输入命令（host / ask / reply / observer / vlan / which），Ctrl+D 结束：",
                        run_arp) != 0;
}
