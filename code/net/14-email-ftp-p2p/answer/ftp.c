#include "line.h"
#include "run_ftp.h"

int main(void) {
    const char *hint =
        "请输入命令（mode / login / list / get / put / quit / claim / ask / go / reset），Ctrl+D 结束：";

    return command_loop(hint, run_ftp) != 0;
}
