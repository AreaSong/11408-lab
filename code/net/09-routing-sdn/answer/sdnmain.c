#include "run_sdn.h"

#include "scan.h"

int main(void) {
    return command_loop(
               "请输入命令（init / switch / host / link / install / go / fail / south），Ctrl+D 结束：",
               run_sdn) == 0
               ? 0
               : 1;
}
