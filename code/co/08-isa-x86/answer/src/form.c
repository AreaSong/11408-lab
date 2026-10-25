#include "form.h"

int show_kind(FILE *out, int naddr) {
    fprintf(out, "【指令格式】\n");
    if (naddr == 4) {
        fprintf(out, "四地址：OP A1 A2 A3 A4。(A1) OP (A2) → A3，A4 是下一条指令的地址\n");
        fprintf(out, "取两个源、写回结果都要访存。PC 能自动加「1」以后，A4 是多余的\n");
        fprintf(out, "结果：下一条地址在 A4\n");
        return 0;
    }
    if (naddr == 3) {
        fprintf(out, "三地址：OP A1 A2 A3。(A1) OP (A2) → A3，两个源操作数都还在\n");
        fprintf(out, "下一条指令的地址由 PC 加上一条指令占的编址单位，这个「1」常常不是数值 1\n");
        fprintf(out, "结果：结果写入 A3\n");
        return 0;
    }
    if (naddr == 2) {
        fprintf(out, "二地址：OP A1 A2。(A1) OP (A2) → A1，结果覆盖目的操作数\n");
        fprintf(out, "下一条地址仍由 PC 给出。这是指令条数里最常见的一类\n");
        fprintf(out, "结果：结果写回 A1\n");
        return 0;
    }
    if (naddr == 1) {
        fprintf(out, "一地址：OP A。(ACC) OP (A) → ACC，另一个操作数和结果都隐含在累加器里\n");
        fprintf(out, "自增、自减也是一地址：A 既是源也是目的\n");
        fprintf(out, "结果：结果写回 ACC\n");
        return 0;
    }
    if (naddr == 0) {
        fprintf(out, "零地址：只有 OP。停机、空操作没有操作数\n");
        fprintf(out, "堆栈机的运算指令也是零地址：弹出栈顶和次栈顶，结果再压回栈顶\n");
        fprintf(out, "结果：操作数隐含在栈顶\n");
        return 0;
    }
    return -1;
}
