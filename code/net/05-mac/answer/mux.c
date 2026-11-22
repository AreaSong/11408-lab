#include "scan.h"
#include "show_mux.h"

int main(void) {
    return lab_lines("请输入命令（stdm / card），Ctrl+D 结束：", mux_command);
}
