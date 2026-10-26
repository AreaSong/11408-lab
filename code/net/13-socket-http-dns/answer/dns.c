#include "line.h"
#include "run_dns.h"

int main(void) {
    const char *hint = "请输入命令（name / server / mode / cache / go / reset），Ctrl+D 结束：";

    return command_loop(hint, run_dns) != 0;
}
