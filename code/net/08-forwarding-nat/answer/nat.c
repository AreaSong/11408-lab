#include "line.h"
#include "run_nat.h"

int main(void) {
    return command_loop("请输入命令（名字 map / basic / pool / emit / out / in），Ctrl+D 结束：", run_nat) != 0;
}
