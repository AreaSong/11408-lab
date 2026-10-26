#include "line.h"
#include "run_page.h"

int main(void) {
    const char *hint =
        "请输入命令（dns / objects / mode / parallel / from / html / obj / rtt / go / reset），Ctrl+D 结束：";

    return command_loop(hint, run_page) != 0;
}
