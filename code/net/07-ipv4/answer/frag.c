#include "loop.h"
#include "run_frag.h"

int main(void) {
    return command_loop("请输入命令（frag），Ctrl+D 结束：", run_frag);
}
