/*
 * CRC 的发送和接收。
 * 模 2 除法的每一步都打出来：最高位是 1 才异或除数，余数才是 FCS。
 */
#include "scan.h"
#include "show_crc.h"

int main(void) {
    return lab_lines("请输入命令（send / check），Ctrl+D 结束：", crc_command);
}
