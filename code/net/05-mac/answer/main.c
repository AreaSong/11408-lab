#include "scan.h"
#include "show_cdma.h"

int main(void) {
    return lab_lines("请输入命令（chip / ortho / recv / send / mix / decode），Ctrl+D 结束：", cdma_command);
}
