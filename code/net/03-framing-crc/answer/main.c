/*
 * 组帧和奇偶校验。
 * CRC 在 crc.c，海明码在 hamming.c。
 */
#include "scan.h"
#include "show_frame.h"

int main(void) {
    return lab_lines("请输入命令（count / byte / bit / parity），Ctrl+D 结束：", frame_command);
}
