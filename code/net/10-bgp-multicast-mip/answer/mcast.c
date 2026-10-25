#include "run_mcast.h"
#include "scan.h"

int main(void) {
    return command_loop("请输入命令（map / same），Ctrl+D 结束：", run_mcast) != 0;
}
