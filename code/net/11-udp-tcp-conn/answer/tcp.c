#include "run_conn.h"
#include "scan.h"

int main(void) {
    tcp_session_init();
    return command_loop("请输入命令（open / send / close），Ctrl+D 结束：", run_conn) != 0;
}
