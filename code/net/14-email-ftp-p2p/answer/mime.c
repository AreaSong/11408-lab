#include "line.h"
#include "run_mime.h"

int main(void) {
    const char *hint = "请输入命令（from / to / subject / body / hex / go / reset），Ctrl+D 结束：";

    return command_loop(hint, run_mime) != 0;
}
