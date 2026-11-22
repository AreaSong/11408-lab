#include "scan.h"
#include "show_access.h"

int main(void) {
    return lab_lines(
        "请输入命令（pure / slot / max / one / non / p / ifs / line / nav），Ctrl+D 结束：",
        access_command);
}
