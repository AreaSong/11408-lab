/*
 * 海明码的校验位数、编码和纠一位错。
 * 位号从左往右、从 1 开始，校验位占 1、2、4、8……。
 */
#include "scan.h"
#include "show_ham.h"

int main(void) {
    return lab_lines("请输入命令（need / encode / correct / flip），Ctrl+D 结束：", ham_command);
}
