#include "exam.h"

#include "beat.h"
#include "psw.h"

#include <stdio.h>

int fetch_clocks(int mem_clocks) {
    /* 送地址并发读 1 拍，主存延迟单独计，再 1 拍把 MDR 送进 IR。 */
    return mem_clocks < 0 ? -1 : 1 + mem_clocks + 1;
}

int gpr_count(int index_bits) {
    if (index_bits < 0 || index_bits > 16) {
        return -1;
    }
    return 1 << index_bits;
}

static void show_flags(FILE *out) {
    uint16_t sum;
    Psw add;
    Psw sub;
    uint16_t diff;
    psw_add(0x7FFF, 0x0001, &sum, &add);
    diff = sub_bits(0x8000, 0x0001, &sub);
    fprintf(out, "SF = F15\n");
    fprintf(out, "A+B 的 OF = A15·B15·非F15 + 非A15·非B15·F15\n");
    fprintf(out, "A-B 的 OF = A15·非B15·非F15 + 非A15·B15·F15\n");
    fprintf(out, "例：7FFFH+0001H=%04XH，SF=%d OF=%d\n", sum, add.sf, add.of);
    fprintf(out, "例：8000H-0001H=%04XH，SF=%d OF=%d\n", diff, sub.sf, sub.of);
}

int show_2022(FILE *out) {
    int regs = gpr_count(4);
    int clocks = fetch_clocks(5);
    int bad;
    static const Micro fetch[] = {
        {"取指", "(PC)->MAR 并发出 Read", "这一拍不计入主存的 5 拍", "PCout MARin Read"},
        {"取指", "(MDR)->IR", "主存读完之后", "MDRout IRin"},
    };
    fprintf(out, "【2022-43】数据通路\n");
    show_flags(out);
    fprintf(out, "暂存器 Y：内总线一次只能送一个操作数，先把 ALU 的一个输入锁住\n");
    fprintf(out, "暂存器 Z：算出结果时总线还被另一个输入占着，输出要先锁住\n");
    fprintf(out, "rs、rd 各 4 位，来自 IR，通用寄存器最多 %d 个\n", regs);
    fprintf(out, "rd 接地址译码器，选中要写的寄存器；rs 接多路选择器，选中要读的寄存器\n");
    fprintf(out, "取指阶段不考虑 PC 增量\n");
    bad = micro_print(out, fetch, 2, 1);
    fprintf(out, "主存从 Read 到数据进入 MDR：5 个时钟周期\n");
    fprintf(out, "取指至少 1+5+1=%d 个时钟周期\n", clocks);
    fprintf(out, "控制信号由 CU 产生。IR 和 FR 的输出接到 CU 的输入端\n");
    fprintf(out, "结果：SF=F15；寄存器 %d 个；取指 %d 个时钟周期\n", regs, clocks);
    return bad ? 1 : 0;
}
