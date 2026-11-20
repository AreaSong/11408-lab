#include "run_ls.h"

#include "scan.h"

int main(void) {
    return command_loop("请输入命令（init / node / link / from / default），Ctrl+D 结束：", run_ls) == 0
               ? 0
               : 1;
}
