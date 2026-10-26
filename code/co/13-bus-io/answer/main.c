/*
 * 总线带宽、I/O 占用、多重中断和握手。每次读一条命令，读到文件结束为止。
 * 算式在 src/ 里打印，这里只负责把命令分出去。
 */
#include <stdio.h>
#include <string.h>

#include "bus.h"
#include "flow.h"
#include "io.h"
#include "nest.h"
#include "prompt.h"
#include "wire.h"

int main(void) {
    char cmd[16];
    for (;;) {
        int rc;
        prompt("请输入命令（bw / txn / poll / query / intr / fit / dma / serial / task / "
               "steal / mask / nest / flow / wire，Ctrl+D 结束）：");
        if (scanf("%15s", cmd) != 1) {
            break;
        }
        rc = bus_command(cmd);
        if (rc == -1) {
            rc = io_command(cmd);
        }
        if (rc == -1) {
            rc = nest_command(cmd);
        }
        if (rc == -1) {
            rc = flow_command(cmd);
        }
        if (rc == -1) {
            rc = wire_command(cmd);
        }
        if (rc == -1) {
            fprintf(stderr, "未知命令：%s\n", cmd);
            return 1;
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
