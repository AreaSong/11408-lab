#include "run_mip.h"
#include "scan.h"

int main(void) {
    mip_session_init();
    return command_loop("请输入命令（home / foreign / colocated / cn / away / back / send / reply），Ctrl+D 结束：",
                        run_mip) != 0;
}
