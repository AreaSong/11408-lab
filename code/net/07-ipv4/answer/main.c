#include "loop.h"
#include "run_hdr.h"

#include <stdio.h>

int main(void) {
    int rc = command_loop("请输入命令（hdr / capture / pkt / origin / end），Ctrl+D 结束：", run_hdr);

    if (rc == 0 && hdr_pending()) {
        fprintf(stderr, "捕获还没有 end\n");
        return 1;
    }
    return rc;
}
