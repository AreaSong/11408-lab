#include "asm.h"
#include "exam.h"
#include "prompt.h"
#include "run.h"

#include <stdio.h>
#include <string.h>

static void trim(char *text) {
    char *start = text;
    char *end;
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }
    end = text + strlen(text);
    while (end > text && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
}

static int add_reg(Program *prog, int index, long value) {
    if (index < 0 || index > 3) {
        fprintf(stderr, "寄存器编号要在 0 到 3\n");
        return -1;
    }
    prog->reg[index] = (uint16_t)value;
    prog->reg_mask |= 1 << index;
    return 0;
}

static int add_data(Program *prog, long addr, long value) {
    if (addr < 0 || addr >= MEM_WORDS || prog->ndata >= MAX_CODE) {
        fprintf(stderr, "内存地址要在 0 到 %d\n", MEM_WORDS - 1);
        return -1;
    }
    prog->daddr[prog->ndata] = (uint16_t)addr;
    prog->dval[prog->ndata] = (uint16_t)value;
    prog->ndata++;
    return 0;
}

static int read_data_line(const char *line, Program *prog) {
    char left[32];
    char right[32];
    long value;
    if (sscanf(line, "%31s %31s", left, right) != 2 || parse_num(right, &value) != 0) {
        fprintf(stderr, "内存行要写成「地址 值」或「r0 值」：%s\n", line);
        return -1;
    }
    if ((left[0] == 'r' || left[0] == 'R') && left[1] >= '0' && left[1] <= '3' && left[2] == '\0') {
        return add_reg(prog, left[1] - '0', value);
    }
    {
        long addr;
        if (parse_num(left, &addr) != 0) {
            return -1;
        }
        return add_data(prog, addr, value);
    }
}

static int read_prog(Program *prog) {
    char line[128];
    prompt("请输入指令，以 end 结束：\n");
    while (fgets(line, sizeof line, stdin) != NULL) {
        trim(line);
        if (line[0] == '\0') {
            continue;
        }
        if (strcmp(line, "end") == 0) {
            return prog->ncode > 0 ? 0 : -1;
        }
        if (prog->ncode >= MAX_CODE || asm_encode(line, &prog->code[prog->ncode]) != 0) {
            fprintf(stderr, "无法汇编：%s\n", line);
            return -1;
        }
        prog->ncode++;
    }
    fprintf(stderr, "指令还没写完\n");
    return -1;
}

static int read_data(Program *prog) {
    char line[128];
    prompt("请输入内存或寄存器，以 end 结束：\n");
    while (fgets(line, sizeof line, stdin) != NULL) {
        trim(line);
        if (line[0] == '\0') {
            continue;
        }
        if (strcmp(line, "end") == 0) {
            return 0;
        }
        if (read_data_line(line, prog) != 0) {
            return -1;
        }
    }
    fprintf(stderr, "内存初值还没写完\n");
    return -1;
}

static int cmd_prog(void) {
    Program prog;
    Cpu cpu;
    memset(&prog, 0, sizeof prog);
    if (read_prog(&prog) != 0 || read_data(&prog) != 0) {
        return 1;
    }
    return cpu_exec(&prog, &cpu, stdout) == 0 ? 0 : 1;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "prog") == 0) {
        return cmd_prog();
    }
    if (strcmp(cmd, "2009-44") == 0) {
        return show_2009(stdout);
    }
    if (strcmp(cmd, "2015-43") == 0) {
        return show_2015_path(stdout);
    }
    if (strcmp(cmd, "2015-44") == 0) {
        return show_2015_code(stdout);
    }
    if (strcmp(cmd, "2022-43") == 0) {
        return show_2022(stdout);
    }
    if (strcmp(cmd, "cycle") == 0) {
        return show_cycle(stdout);
    }
    return show_pick(stdout, cmd);
}

int main(void) {
    char line[128];
    for (;;) {
        prompt("请输入命令：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;
        }
        trim(line);
        if (line[0] == '\0') {
            continue;
        }
        if (dispatch(line) != 0) {
            return 1;
        }
    }
}
