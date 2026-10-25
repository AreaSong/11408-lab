/*
 * 码元速率、奈奎斯特与香农极限。
 * 波形在 wave.c：NRZ、曼彻斯特、差分曼彻斯特。
 */
#include <stdio.h>
#include <string.h>

#include "console.h"
#include "prompt.h"

static int too_long(const char *line) {
    return strchr(line, '\n') == NULL && !feof(stdin);
}

static int blank_line(const char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') {
        line++;
    }
    return *line == '\0';
}

int main(void) {
    char line[512];
    int first = 1;

    while (1) {
        prompt("请输入命令（convert / nyquist / shannon / bound），Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        if (too_long(line)) {
            fprintf(stderr, "这一行太长\n");
            return 1;
        }
        if (blank_line(line)) {
            continue;
        }
        if (!first) {
            fputc('\n', stdout);
        }
        first = 0;
        if (run_command(line) != 0) {
            return 1;
        }
    }
    return 0;
}
