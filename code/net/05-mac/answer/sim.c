#include "scan.h"
#include "show_bus.h"

int main(void) {
    return lab_lines("请输入命令（trace / compare），Ctrl+D 结束：", bus_command);
}
