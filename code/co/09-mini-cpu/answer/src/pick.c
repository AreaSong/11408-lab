#include "exam.h"

#include "run.h"

#include <stdio.h>
#include <string.h>

int min_machine_cycles(void) {
    return 2;
}

int reg_visible(const char *name) {
    static const char *yes[] = {"pc", "r0", "r1", "r2", "r3", "psw", "base", "flag", "acc", NULL};
    static const char *no[] = {"mar", "mdr", "ir", "t", "sr", "mir", NULL};
    int i;
    for (i = 0; yes[i] != NULL; i++) {
        if (strcmp(name, yes[i]) == 0) {
            return 1;
        }
    }
    for (i = 0; no[i] != NULL; i++) {
        if (strcmp(name, no[i]) == 0) {
            return 0;
        }
    }
    return -1;
}

static char letter_of(int value, const int *option, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (option[i] == value) {
            return (char)('A' + i);
        }
    }
    return '?';
}

static char letter_2010(void) {
    static const char *opt[] = {"mar", "pc", "mdr", "ir"};
    int i;
    int found = -1;
    for (i = 0; i < 4; i++) {
        if (reg_visible(opt[i]) == 1) {
            found = found < 0 ? i : -2;
        }
    }
    return found < 0 ? '?' : (char)('A' + found);
}

static int mask_bits(int a, int b, int c, int d) {
    return (a ? 1 : 0) | (b ? 2 : 0) | (c ? 4 : 0) | (d ? 8 : 0);
}

static char letter_2016(void) {
    int pc = addr_bits((1ULL << 32) / 4);
    int ir = 32;
    static const int opt[] = {30 * 100 + 30, 30 * 100 + 32, 32 * 100 + 30, 32 * 100 + 32};
    return letter_of(pc * 100 + ir, opt, 4);
}

static int nop_pc(void) {
    Program prog;
    Cpu cpu;
    FILE *fp = tmpfile();
    memset(&prog, 0, sizeof prog);
    prog.code[1] = (uint16_t)(OP_HLT << 12);
    prog.ncode = 2;
    if (fp == NULL || cpu_exec(&prog, &cpu, fp) != 0) {
        if (fp != NULL) {
            fclose(fp);
        }
        return -1;
    }
    fclose(fp);
    return (int)cpu.pc;
}

static char letter_2019(void) {
    /* 取数和执行：寄存器、ALU、主存都要用，译码器已经用过了。 */
    static const int opt[] = {3, 7, 14, 13};
    return letter_of(mask_bits(1, 1, 1, 0), opt, 4);
}

static char letter_2021_reg(void) {
    static const int opt[] = {3, 9, 10, 12};
    int mask = mask_bits(reg_visible("ir") == 1, reg_visible("mir") == 1, reg_visible("base") == 1,
                         reg_visible("flag") == 1);
    return letter_of(mask, opt, 4);
}

static char letter_2023(void) {
    /* I ALU、IV MUX 是操作元件；II PC、III 通用寄存器是状态元件。 */
    static const int opt[] = {3, 9, 6, 11};
    return letter_of(mask_bits(1, 0, 0, 1), opt, 4);
}

char pick_letter(const char *id) {
    if (strcmp(id, "2009-11") == 0) {
        return 'C';
    }
    if (strcmp(id, "2010-18") == 0) {
        return letter_2010();
    }
    if (strcmp(id, "2011-19") == 0) {
        return nop_pc() > 0 ? 'C' : '?';
    }
    if (strcmp(id, "2016-18") == 0) {
        return letter_2016();
    }
    if (strcmp(id, "2016-20") == 0) {
        return 'A';
    }
    if (strcmp(id, "2019-17") == 0) {
        return letter_2019();
    }
    if (strcmp(id, "2021-17") == 0) {
        return letter_2021_reg();
    }
    if (strcmp(id, "2021-18") == 0) {
        return 'C';
    }
    if (strcmp(id, "2023-18") == 0) {
        return letter_2023();
    }
    return '?';
}

static int finish(FILE *out, const char *id) {
    char letter = pick_letter(id);
    if (letter == '?') {
        fprintf(out, "结果：无法对应选项\n");
        return 1;
    }
    fprintf(out, "结果：答案 %c\n", letter);
    return 0;
}

static int show_2009_11(FILE *out) {
    fprintf(out, "【2009-11】指令和数据都是二进制\n");
    fprintf(out, "取指阶段按 PC 读出的是指令，执行阶段按地址码读出的是数据\n");
    fprintf(out, "还没确认读出的是指令，就不能拿操作码去译码\n");
    fprintf(out, "存放单元本身没有标记，寻址方式也不负责区分\n");
    return finish(out, "2009-11");
}

static int show_2010(FILE *out) {
    fprintf(out, "【2010-18】汇编程序员可见的寄存器\n");
    fprintf(out, "PC 能被转移、调用改掉，程序员看得到\n");
    fprintf(out, "MAR、MDR、IR 只在取指和访存时由硬件使用\n");
    return finish(out, "2010-18");
}

static int show_2011(FILE *out) {
    int pc = nop_pc();
    fprintf(out, "【2011-19】指令周期里错误的叙述\n");
    fprintf(out, "不预取、不用 Cache 时，每条指令至少为取指访问内存一次\n");
    fprintf(out, "指令周期由若干时钟周期组成，至少 1 个\n");
    fprintf(out, "空操作加停机执行完，PC=%d。取指已经把 PC 加 1\n", pc);
    fprintf(out, "开中断时，外部中断在每条指令执行结束时检查\n");
    return finish(out, "2011-19");
}

static int show_2016_bits(FILE *out) {
    int pc = addr_bits((1ULL << 32) / 4);
    int mar = addr_bits(1ULL << 32);
    fprintf(out, "【2016-18】PC 和 IR 的位数\n");
    fprintf(out, "主存 4 GB = 2^32 B，按字节编址\n");
    fprintf(out, "指令 32 bit = 4 B，按字边界对齐\n");
    fprintf(out, "指令条数 = 2^32 B / 4 B = 2^30 条，PC 至少 %d 位\n", pc);
    fprintf(out, "IR 要放下整条指令，至少 32 位\n");
    fprintf(out, "MAR 仍要字节地址，至少 %d 位。本题没问 MAR\n", mar);
    return finish(out, "2016-18");
}

static int show_2016_bus(FILE *out) {
    fprintf(out, "【2016-20】单周期处理器\n");
    fprintf(out, "单总线一个时钟周期只能传一个数据。取指、读数、写回要分几次传，所以是多周期\n");
    fprintf(out, "时钟周期被最慢的指令拉长，主频较低。CPI 为 1\n");
    fprintf(out, "控制信号在这一个时钟周期里保持不变\n");
    return finish(out, "2016-20");
}

static int show_2019(FILE *out) {
    fprintf(out, "【2019-17】R[r2]<-R[r1]+M[R[r0]]\n");
    fprintf(out, "取数要用通用寄存器组和主存，相加要用 ALU\n");
    fprintf(out, "指令译码器在译码时用，取数和执行不再用它\n");
    return finish(out, "2019-17");
}

static int show_2021_reg(FILE *out) {
    fprintf(out, "【2021-17】哪些寄存器程序员可见\n");
    fprintf(out, "基址寄存器、标志寄存器程序员用得着\n");
    fprintf(out, "指令寄存器和微指令寄存器都在控制器内部\n");
    return finish(out, "2021-17");
}

static int show_2021_path(FILE *out) {
    fprintf(out, "【2021-18】数据通路包含什么\n");
    fprintf(out, "数据通路含 ALU 这类操作元件，也含寄存器这类状态元件\n");
    fprintf(out, "数据怎么流，由控制信号决定\n");
    fprintf(out, "异常检测也在指令执行的路径上，属于数据通路\n");
    return finish(out, "2021-18");
}

static int show_2023(FILE *out) {
    fprintf(out, "【2023-18】操作元件和状态元件\n");
    fprintf(out, "ALU、MUX 的输出只取决于当前输入，是操作元件\n");
    fprintf(out, "PC、通用寄存器能记住状态，是状态元件\n");
    return finish(out, "2023-18");
}

int show_cycle(FILE *out) {
    fprintf(out, "【指令周期的阶段】\n");
    fprintf(out, "机器周期可以有四个：取指、间址、执行、中断\n");
    fprintf(out, "至少 %d 个：取指和执行。间址只有间接寻址才进入\n", min_machine_cycles());
    fprintf(out, "中断周期在指令执行结束、并且有中断请求时才进入\n");
    fprintf(out, "五段流水用的是另一套名字：取指、译码、执行、访存、写回\n");
    fprintf(out, "结果：至少 %d 个机器周期\n", min_machine_cycles());
    return 0;
}

int show_pick(FILE *out, const char *id) {
    if (strcmp(id, "2009-11") == 0) {
        return show_2009_11(out);
    }
    if (strcmp(id, "2010-18") == 0) {
        return show_2010(out);
    }
    if (strcmp(id, "2011-19") == 0) {
        return show_2011(out);
    }
    if (strcmp(id, "2016-18") == 0) {
        return show_2016_bits(out);
    }
    if (strcmp(id, "2016-20") == 0) {
        return show_2016_bus(out);
    }
    if (strcmp(id, "2019-17") == 0) {
        return show_2019(out);
    }
    if (strcmp(id, "2021-17") == 0) {
        return show_2021_reg(out);
    }
    if (strcmp(id, "2021-18") == 0) {
        return show_2021_path(out);
    }
    if (strcmp(id, "2023-18") == 0) {
        return show_2023(out);
    }
    fprintf(stderr, "没有这道题：%s\n", id);
    return 1;
}
