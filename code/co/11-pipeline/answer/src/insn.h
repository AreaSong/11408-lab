#ifndef PIPE_INSN_H
#define PIPE_INSN_H

/*
 * 五段流水线里的一条指令。
 * 只记录冒险要用的寄存器：谁写、谁在 EX 段读、谁要到 MEM 段才读。
 * 存储单元之间的相关不在这里判断，408 这几道题的数据相关都在寄存器上。
 */

enum {
    INSN_N = 24,
    INSN_NAME = 8
};

typedef struct Insn {
    char text[72];
    int rd;
    int ex[2];
    int nex;
    char ex_name[2][INSN_NAME];
    int memsrc[1];
    int nmem;
    char mem_name[INSN_NAME];
    int uses_mem;
    int is_load;
    int is_branch;
    int taken;
    int target;
} Insn;

/* 成功返回指令条数，失败返回 -1，并把原因写进 err。 */
int insn_parse(const char *src, Insn *out, int cap, char *err, int errlen);

/* later 读取 earlier 写入的寄存器，就是 RAW。earlier 必须在 later 前面。 */
int insn_raw(const Insn *ins, int earlier, int later);

#endif
