#include "exam.h"

#include "metric.h"

#include <stdio.h>
#include <string.h>

static const char SRC_AB[] =
    "load R1, [a]\n"
    "load R2, [b]\n"
    "add R1, R2\n"
    "store R2, [x]\n";

static const char SRC_MUL[] =
    "load R1, [x]\n"
    "load R2, [a]\n"
    "shl R1\n"
    "add R1, R2\n"
    "store R2, [x]\n";

static const char SRC_2014[] =
    "sll R4, R2, 2\n"
    "add R4, R4, R3\n"
    "load R5, 0(R4)\n"
    "add R1, R1, R5\n"
    "add R2, R2, 1\n"
    "bne R2, R6, loop\n"
    "loop: sll R4, R2, 2\n";

static const char SRC_2016[] =
    "add R1, R2, R3\n"
    "add R5, R2, R4\n"
    "add R4, R5, R3\n"
    "add R5, R2, R6\n";

static const char SRC_2019[] =
    "add s2, s1, s0\n"
    "load s3, 0(t2)\n"
    "add s2, s2, s3\n"
    "store s2, 0(t2)\n";

static const char SRC_2023[] =
    "add s2, s1, s0\n"
    "load s3, 0(s2)\n"
    "beq t2, s3, L1\n"
    "addi t2, t2, 20\n";

static const char SRC_RAW[] =
    "add R1, R2, R3\n"
    "sub R4, R1, R5\n"
    "and R6, R1, R7\n"
    "or R8, R1, R9\n";

static const char SRC_LOAD[] =
    "load R1, 0(R2)\n"
    "add R3, R1, R4\n";

static const char SRC_STRUCT[] =
    "load R1, [a]\n"
    "add R2, R3, R4\n"
    "add R5, R6, R7\n"
    "add R8, R9, R10\n";

static const char SRC_TAKEN[] =
    "beq R1, R2, L\n"
    "add R3, R4, R5\n"
    "add R6, R7, R8\n"
    "L: nop\n";

static const char SRC_NOT[] =
    "beq R1, R2, not\n"
    "add R3, R4, R5\n"
    "add R6, R7, R8\n";

static const char SRC_SLOT[] =
    "beq R1, R2, L\n"
    "add R3, R4, R5\n"
    "L: nop\n";

static int run_src(const char *mode, const char *src, PipeStat *st, FILE *fp) {
    Insn buf[INSN_N];
    PipeOpt opt;
    char err[160];
    int n;
    if (pipe_opt_named(mode, &opt) != 0) {
        return -1;
    }
    n = insn_parse(src, buf, INSN_N, err, (int)sizeof err);
    if (n < 0 || pipe_run(buf, n, &opt, st) != 0) {
        if (fp != NULL) {
            fprintf(fp, "%s\n", n < 0 ? err : "流水线没有跑完");
        }
        return -1;
    }
    if (fp != NULL) {
        pipe_report(fp, buf, n, &opt, st);
    }
    return 0;
}

int exam_sim(const char *key, PipeStat *st) {
    if (strcmp(key, "2012-ab") == 0) {
        return run_src("late", SRC_AB, st, NULL);
    }
    if (strcmp(key, "2012-mul") == 0) {
        return run_src("late", SRC_MUL, st, NULL);
    }
    if (strcmp(key, "2014") == 0) {
        return run_src("half-branch", SRC_2014, st, NULL);
    }
    if (strcmp(key, "2016") == 0) {
        return run_src("half", SRC_2016, st, NULL);
    }
    if (strcmp(key, "2019") == 0) {
        return run_src("forward", SRC_2019, st, NULL);
    }
    if (strcmp(key, "2023") == 0) {
        return run_src("branch", SRC_2023, st, NULL);
    }
    if (strcmp(key, "struct") == 0) {
        return run_src("struct", SRC_STRUCT, st, NULL);
    }
    if (strcmp(key, "raw-half") == 0) {
        return run_src("half", SRC_RAW, st, NULL);
    }
    if (strcmp(key, "raw-forward") == 0) {
        return run_src("forward", SRC_RAW, st, NULL);
    }
    if (strcmp(key, "load-use") == 0) {
        return run_src("forward", SRC_LOAD, st, NULL);
    }
    if (strcmp(key, "predict-taken") == 0) {
        return run_src("predict", SRC_TAKEN, st, NULL);
    }
    if (strcmp(key, "predict-not") == 0) {
        return run_src("predict", SRC_NOT, st, NULL);
    }
    if (strcmp(key, "slot") == 0) {
        return run_src("slot", SRC_SLOT, st, NULL);
    }
    return -1;
}

int exam_letter(const char *name) {
    if (strcmp(name, "2009-18") == 0) {
        return 'A';
    }
    if (strcmp(name, "2010-19") == 0) {
        return 'A';
    }
    if (strcmp(name, "2011-18") == 0) {
        return 'D';
    }
    if (strcmp(name, "2013-18") == 0) {
        return 'C';
    }
    if (strcmp(name, "2014-16") == 0) {
        return 'D';
    }
    if (strcmp(name, "2016-19") == 0) {
        return 'B';
    }
    if (strcmp(name, "2017-17") == 0) {
        return 'C';
    }
    if (strcmp(name, "2018-20") == 0) {
        return 'D';
    }
    if (strcmp(name, "2019-18") == 0) {
        return 'C';
    }
    if (strcmp(name, "2020-17") == 0) {
        return 'B';
    }
    if (strcmp(name, "2023-19") == 0) {
        return 'C';
    }
    if (strcmp(name, "2024-19") == 0) {
        return 'C';
    }
    return 0;
}

static void show_pairs(FILE *fp, const char *src) {
    Insn buf[INSN_N];
    char err[160];
    int n = insn_parse(src, buf, INSN_N, err, (int)sizeof err);
    int i;
    int j;
    if (n < 0) {
        fprintf(fp, "%s\n", err);
        return;
    }
    fputs("RAW 指令对：", fp);
    for (i = 0; i < n; i++) {
        for (j = i + 1; j < n; j++) {
            if (insn_raw(buf, i, j)) {
                fprintf(fp, " I%d-I%d", i + 1, j + 1);
            }
        }
    }
    fputc('\n', fp);
}

static void show_2009(FILE *fp) {
    long long stage[] = {90, 80, 70, 60};
    fputs("【2009-18】四段时间 90、80、70、60 ns，忽略段间缓存\n", fp);
    fputs("同步流水线用同一拍，时钟至少等于最长的功能段，否则那一段做不完\n", fp);
    metric_clock(fp, stage, 4, 0, "ns");
    fputs("题目已经说忽略段间缓存，锁存按 0 ns，不再加一截\n", fp);
    fputs("B、C、D 都比最长段短，流水线会被 90 ns 那段堵住\n", fp);
    fputs("结果：答案 A\n", fp);
}

static void show_2018(FILE *fp) {
    long long stage[] = {80, 50, 50, 70, 50};
    fputs("【2018-20】五段 80、50、50、70、50 ps，流水段寄存器 20 ps\n", fp);
    metric_clock(fp, stage, 5, 20, "ps");
    fputs("A 的 60 ps 对不上任何一段。B 的 70 ps 是次长段。C 的 80 ps 漏了锁存\n", fp);
    fputs("结果：答案 D\n", fp);
}

static void show_2013(FILE *fp) {
    fputs("【2013-18】4 段流水线，主频 1.03 GHz，100 条指令，没有阻塞\n", fp);
    metric_freq(fp, 4, 100, 1030);
    fputs("A 的 0.25×10^9 是把 4 段都算进 CPI，再按 1 GHz 去除\n", fp);
    fputs("B 的 0.97×10^9 接近 100/103×10^9，主频误用了 1 GHz\n", fp);
    fputs("D 的 1.03×10^9 就是主频本身，没算上填流水线的 3 拍\n", fp);
    fputs("结果：答案 C\n", fp);
}

static void show_2010(FILE *fp) {
    PipeStat half;
    PipeStat fwd;
    fputs("【2010-19】不会引起指令流水线阻塞的是\n", fp);
    fputs("结构相关、数据相关、控制相关都会让后面的指令赶不上原来的节拍\n", fp);
    fputs("数据旁路是把已经算出来的结果提前送过去，用来去掉数据相关造成的停顿\n", fp);
    fputs("相邻两条都用 R1，无转发、先写后读：\n", fp);
    run_src("half", "add R1, R2, R3\nsub R4, R1, R5\n", &half, fp);
    fputs("同样两条，加上转发：\n", fp);
    run_src("forward", "add R1, R2, R3\nsub R4, R1, R5\n", &fwd, fp);
    fprintf(fp, "无转发多 %d 拍，转发后多 %d 拍\n", half.extra, fwd.extra);
    fputs("所以旁路本身不引起阻塞。数据相关、条件转移、资源冲突都会\n", fp);
    fputs("结果：答案 A\n", fp);
}

static void show_2011(FILE *fp) {
    fputs("【2011-18】有利于实现指令流水线的是\n", fp);
    fputs("Ⅰ 指令格式规整、长度一致：取指和译码好切成固定的段\n", fp);
    fputs("Ⅱ 指令和数据按边界对齐：一次访存就能取完整的字，不用跨边界拼\n", fp);
    fputs("Ⅲ 只有 Load/Store 访存：其余指令在寄存器里做完，EX 和 MEM 的职责清楚，也少结构冒险\n", fp);
    fputs("三条都有利\n", fp);
    fputs("结果：答案 D\n", fp);
}

static void show_2017(FILE *fp) {
    fputs("【2017-17】超标量流水线的特性\n", fp);
    fputs("超标量是多套功能部件，一个时钟周期发射多条指令，用空间换时间\n", fp);
    fputs("Ⅰ 缩短功能段的处理时间，那是把段再细分的超流水线，不是超标量\n", fp);
    fputs("Ⅱ 一个时钟周期发射多条，是超标量的定义\n", fp);
    fputs("Ⅲ 可以再配动态调度，让没有相关的指令先走。动态调度本身超出本节的模拟\n", fp);
    fputs("动态流水线是另一件事：同一时间各段可以做不同运算。静态流水线同一时间只能一种接法\n", fp);
    fputs("结果：答案 C\n", fp);
}

static void show_2020(FILE *fp) {
    fputs("【2020-17】理想 CPI 为 1 的处理器\n", fp);
    fputs("CPI 是平均每条指令占用的时钟周期数\n", fp);
    fputs("Ⅰ 单周期：一条指令占一个长时钟，CPI = 1\n", fp);
    fputs("Ⅱ 多周期：一条指令拆成多拍，CPI > 1\n", fp);
    fputs("Ⅲ 基本流水线：理想情况下每个时钟流出一条，CPI = 1。填流水线和排空不算进这个理想值\n", fp);
    fputs("Ⅳ 超标量：一个时钟流出多条，CPI < 1\n", fp);
    fputs("结果：答案 B\n", fp);
}

static void show_2024(FILE *fp) {
    PipeStat st;
    fputs("【2024-19】5 段 RISC 流水线里，关于数据冒险的叙述，错误的是\n", fp);
    fputs("A 相邻指令写后读，可能冒险\n", fp);
    fputs("B 插入气泡就是多停几拍，数据来得及写回，能避开冒险\n", fp);
    fputs("D 把顺序调开，再不够就插 nop，插够了流水线退回一条接一条，数据冒险都能消掉\n", fp);
    fputs("C 说所有数据冒险都能靠转发解决。load 的数要到 MEM 段末才从存储器出来，下一条 EX 要用，转发也晚一拍：\n", fp);
    run_src("forward", SRC_LOAD, &st, fp);
    fprintf(fp, "这条 load-use 在有转发时仍然阻塞 %d 拍\n", st.extra);
    fputs("结果：答案 C\n", fp);
}

static void show_2014_16(FILE *fp) {
    PipeStat split;
    PipeStat uni;
    fputs("【2014-16】指令 Cache 和数据 Cache 分开的主要目的\n", fp);
    fputs("分开以后取指走指令存储器，load/store 的 MEM 走数据存储器，两种访存可以同一拍\n", fp);
    fputs("不分开时，第 1 条 load 的 MEM 和第 4 条的 IF 会撞在同一拍：\n", fp);
    run_src("struct", SRC_STRUCT, &uni, fp);
    fputs("分开之后，同样四条没有结构阻塞：\n", fp);
    run_src("forward", SRC_STRUCT, &split, fp);
    fprintf(fp, "不分开多 %d 拍，分开后多 %d 拍\n", uni.extra, split.extra);
    fputs("缺失率、命中率、缺失损失都没有因为分开而改变\n", fp);
    fputs("结果：答案 D\n", fp);
}

static void show_2016(FILE *fp) {
    PipeStat st;
    fputs("【2016-19】无转发的五段流水线，哪一对有数据冒险\n", fp);
    fputs("数据冒险在这里按 RAW：后面读寄存器，前面还没写完\n", fp);
    show_pairs(fp, SRC_2016);
    fputs("I2 和 I4 都写 R5，是 WAW。按序流水线写回不会反序，这道题不把它算进冒险对\n", fp);
    fputs("I1 写 R1，后面没有人读 R1。I3 写 R4，I4 不读 R4\n", fp);
    run_src("half", SRC_2016, &st, fp);
    fprintf(fp, "真正停在 ID 的是 I3，多 %d 拍，因为 I2 要到 WB 才写下 R5\n", st.id_extra[2]);
    fputs("结果：答案 B\n", fp);
}

static void show_2019(FILE *fp) {
    PipeStat st;
    fputs("【2019-18】哪一对不存在数据冒险\n", fp);
    show_pairs(fp, SRC_2019);
    fputs("I1 写 s2，I3 读 s2。I2 写 s3，I3 读 s3，而且 I2 是 load。I3 写 s2，I4 的 store 读 s2\n", fp);
    fputs("I2 写的是 s3，I4 读的是 s2 和 t2，两边没有写后读\n", fp);
    run_src("forward", SRC_2019, &st, fp);
    fprintf(fp, "有转发时，I1 到 I3 的 ALU 结果能直接送过去。I2 到 I3 是 load-use，I3 仍要停 %d 拍\n",
            st.id_extra[2]);
    fputs("I4 要的 s2 在 MEM 段才用，I3 的结果可以从 MEM/WB 转过去，I4 自己不再为数据停\n", fp);
    fputs("结果：答案 C\n", fp);
}

static void show_2023(FILE *fp) {
    PipeStat st;
    fputs("【2023-19】有转发，控制冒险用硬件阻塞，哪些指令发生了阻塞\n", fp);
    run_src("branch", SRC_2023, &st, fp);
    fprintf(fp, "I2 读 s2，I1 在 EX 末就已经把和算出来，转发后 I2 的数据阻塞是 %d 拍\n",
            st.id_extra[1]);
    fprintf(fp, "I3 在 EX 要用 s3，I2 的 load 要到 MEM 末才取到，I3 数据阻塞 %d 拍\n", st.id_extra[2]);
    fprintf(fp, "beq 在 MEM 段才改 PC，I4 的取指因此再晚 %d 拍\n", st.ctrl[3]);
    fputs("阻塞的是 I3 和 I4。I2 被转发消掉了，不能选进答案\n", fp);
    fputs("结果：答案 C\n", fp);
}

static void show_2012(FILE *fp) {
    PipeStat ab;
    PipeStat mul;
    fputs("【2012-44】\n", fp);
    fputs("(1) x = -513，16 位补码：513 = 0201H，取反加一得 FDFFH\n", fp);
    fputs("算术右移 1 位，空出的高位补符号 1，得到 FEFFH。移位过程在第 02 关\n", fp);
    fputs("(2) 4 条指令、5 段、没有任何阻塞：周期数 = 5 + 4 - 1 = 8\n", fp);
    fputs("(3) x = a + b。ADD Rs, Rd 的功能是 (Rs)+(Rd)→Rd，所以和写在 R2，STORE 用 R2\n", fp);
    fputs("数据 Cache 和指令 Cache 是分开的，没有结构冒险。没有转发，而且同一拍不能又读又写\n", fp);
    run_src("late", SRC_AB, &ab, fp);
    fputs("I3 的 ID 要等 I1、I2 都写回，并且写回的那一拍还不能读\n", fp);
    fputs("I4 的 IF 堵住，是因为 I3 还停在 ID，按序发射，下一条不能先取进来覆盖它\n", fp);
    fputs("(4) x = x*2+a。SHL 把 R1 左移，ADD R1, R2 把和写入 R2，再 STORE R2\n", fp);
    run_src("late", SRC_MUL, &mul, fp);
    fprintf(fp, "结果：无阻塞 %d 个时钟周期，x=a+b 共 %d 个时钟周期，x=x*2+a 共 %d 个时钟周期\n",
            metric_cycles(5, 4), ab.cycles, mul.cycles);
}

static void print_id_list(FILE *fp, const PipeStat *st, int n) {
    int i;
    int any = 0;
    for (i = 0; i < n; i++) {
        if (st->id_extra[i] > 0) {
            fprintf(fp, "%sI%d", any ? " " : "", i + 1);
            any = 1;
        }
    }
    if (!any) {
        fputs("无", fp);
    }
}

static void show_2014(FILE *fp) {
    PipeStat st;
    fputs("【2014-44】第（4）问。第（1）（2）（3）问的编址和 OFFSET 在第 08 关\n", fp);
    fputs("无转发，先写后读。bne 无论转移与否都阻塞 3 个时钟周期\n", fp);
    fputs("下面多放了下一次循环的第一条，用来看它会不会再因为 R2 停下来\n", fp);
    run_src("half-branch", SRC_2014, &st, fp);
    fputs("由于数据相关停在 ID 的是：", fp);
    print_id_list(fp, &st, 7);
    fputc('\n', fp);
    fprintf(fp, "I6 是 bne，控制冒险。下一条 I7 的控制阻塞是 %d 拍，自己的数据阻塞是 %d 拍\n",
            st.ctrl[6], st.id_extra[6]);
    fputs("I7 就是下一轮的 sll，它读 R2，I5 写 R2，相关还在\n", fp);
    fputs("I6 先为 R2 停了两拍，后面又空出 3 拍等分支，I5 的 WB 已经过去，所以 I7 不再为数据停\n", fp);
    fputs("I5 的 IF 会跟着 I4 的 ID 停，那是按序发射，不是 I5 自己和谁相关\n", fp);
    fputs("结果：数据阻塞 ", fp);
    print_id_list(fp, &st, 7);
    fprintf(fp, "，控制冒险 I6，下轮 I7 阻塞 %d 拍\n", st.id_extra[6]);
}

int show_exam(FILE *fp, const char *name) {
    if (strcmp(name, "2009-18") == 0) {
        show_2009(fp);
    } else if (strcmp(name, "2010-19") == 0) {
        show_2010(fp);
    } else if (strcmp(name, "2011-18") == 0) {
        show_2011(fp);
    } else if (strcmp(name, "2012-44") == 0) {
        show_2012(fp);
    } else if (strcmp(name, "2013-18") == 0) {
        show_2013(fp);
    } else if (strcmp(name, "2014-16") == 0) {
        show_2014_16(fp);
    } else if (strcmp(name, "2014-44") == 0) {
        show_2014(fp);
    } else if (strcmp(name, "2016-19") == 0) {
        show_2016(fp);
    } else if (strcmp(name, "2017-17") == 0) {
        show_2017(fp);
    } else if (strcmp(name, "2018-20") == 0) {
        show_2018(fp);
    } else if (strcmp(name, "2019-18") == 0) {
        show_2019(fp);
    } else if (strcmp(name, "2020-17") == 0) {
        show_2020(fp);
    } else if (strcmp(name, "2023-19") == 0) {
        show_2023(fp);
    } else if (strcmp(name, "2024-19") == 0) {
        show_2024(fp);
    } else {
        return 1;
    }
    return 0;
}
