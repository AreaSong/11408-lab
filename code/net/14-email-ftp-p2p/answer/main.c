#include "line.h"
#include "run_dist.h"

int main(void) {
    const char *hint = "请输入命令（file / server / peer / count / go / curve / reset），Ctrl+D 结束：";

    return command_loop(hint, run_dist) != 0;
}
