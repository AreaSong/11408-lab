#include "calc.h"
#include "exam.h"
#include "micro.h"
#include "prompt.h"
#include "sim.h"
#include "wire.h"

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

static int starts(const char *text, const char *prefix, const char **rest) {
    size_t n = strlen(prefix);
    if (strncmp(text, prefix, n) != 0) {
        return 0;
    }
    *rest = text + n;
    return 1;
}

static int dispatch(char *line) {
    const char *rest = NULL;
    if (strcmp(line, "wire") == 0) return show_wire(stdout);
    if (strcmp(line, "micro") == 0) return show_micro(stdout);
    if (show_exam(stdout, line) == 0) return 0;
    {
        int scene = show_scene(stdout, line, NULL);
        if (scene == 0) return 0;
        if (scene < 0) return -1;
    }
    if (starts(line, "field ", &rest)) return cmd_field(stdout, rest);
    if (starts(line, "next ", &rest)) return cmd_next(stdout, rest);
    if (starts(line, "rom ", &rest)) return cmd_rom(stdout, rest);
    if (starts(line, "class ", &rest)) return cmd_class(stdout, rest);
    fprintf(stderr, "未知命令：%s\n", line);
    return -1;
}

int main(void) {
    char line[160];
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
