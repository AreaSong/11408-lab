#include "line.h"
#include "run_dhcp.h"

int main(void) {
    return command_loop("请输入命令（dora / frame），Ctrl+D 结束：", run_dhcp) != 0;
}
