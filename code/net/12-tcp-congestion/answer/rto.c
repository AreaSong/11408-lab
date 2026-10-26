#include "line.h"
#include "run_rtt.h"

int main(void) {
    return command_loop("请输入命令（init / sample / rexmit），Ctrl+D 结束：", run_rtt) != 0;
}
