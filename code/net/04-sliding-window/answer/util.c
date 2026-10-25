#include "scan.h"
#include "show_pipe.h"

int main(void) {
    return lab_lines("请输入命令（need / rate / limit / compare），Ctrl+D 结束：", pipe_command);
}
