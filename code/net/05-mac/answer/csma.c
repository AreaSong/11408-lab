#include "scan.h"
#include "show_limit.h"

int main(void) {
    return lab_lines(
        "请输入命令（frame / reach / tau / delta / backoff / picks / detect / goodput），Ctrl+D 结束：",
        limit_command);
}
