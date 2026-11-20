#include "run_rip.h"

#include "scan.h"

int main(void) {
    return command_loop(
               "请输入命令（init / router / net / rlink / nlink / set / learn / converge / "
               "spread / slow / as / ttl），Ctrl+D 结束：",
               run_rip) == 0
               ? 0
               : 1;
}
