#include "line.h"
#include "run_ipv6.h"

int main(void) {
    return command_loop("请输入命令（compress / expand），Ctrl+D 结束：", run_ipv6) != 0;
}
