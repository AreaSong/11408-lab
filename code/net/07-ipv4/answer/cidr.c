#include "loop.h"
#include "run_cidr.h"

int main(void) {
    const char *hint = "请输入命令（addr / class / subnet / split / aggregate / siblings / remain），Ctrl+D 结束：";

    return command_loop(hint, run_cidr);
}
