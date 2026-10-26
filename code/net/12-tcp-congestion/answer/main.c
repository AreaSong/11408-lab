#include "line.h"
#include "run_cong.h"

int main(void) {
    return command_loop("请输入命令（mss / rtt / buf / rwnd / seq / init / timeout / round / send / ack / dup / until / stall），Ctrl+D 结束：",
                        run_cong) != 0;
}
