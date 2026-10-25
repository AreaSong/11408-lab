#include "run_seq.h"
#include "scan.h"

int main(void) {
    return command_loop("请输入命令（cum / reply / app），Ctrl+D 结束：", run_seq) != 0;
}
