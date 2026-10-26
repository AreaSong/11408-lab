#include "line.h"
#include "run_proto.h"

int main(void) {
    const char *hint =
        "请输入命令（stmt / plain / browse / model / transport / hop / webmail / go / reset），Ctrl+D 结束：";

    return command_loop(hint, run_proto) != 0;
}
