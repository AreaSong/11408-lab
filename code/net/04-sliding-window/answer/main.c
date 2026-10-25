#include "scan.h"
#include "show_link.h"

int main(void) {
    return lab_lines(
        "请输入命令（proto / send / arrive / ack / drop / loseack / timeout / show），Ctrl+D 结束：",
        link_command);
}
