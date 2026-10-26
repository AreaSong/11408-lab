#include "line.h"
#include "run_flow.h"

int main(void) {
    return command_loop("请输入命令（mss / cwnd / buf / seq / send / ack / read / probe），Ctrl+D 结束：",
                        run_flow) != 0;
}
