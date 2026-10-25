#include "run_bgp.h"
#include "scan.h"

int main(void) {
    bgp_session_init();
    return command_loop("请输入命令（as / peer / origin / step / encap / session / offer / pick / reset），Ctrl+D 结束：",
                        run_bgp) != 0;
}
