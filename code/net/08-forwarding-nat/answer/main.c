#include "line.h"
#include "run_fwd.h"

int main(void) {
    fwd_init();
    return command_loop("请输入命令（route / default / lookup / ttl / pkt），Ctrl+D 结束：", run_fwd) != 0;
}
