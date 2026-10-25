/*
 * 把比特串画成 NRZ、曼彻斯特、差分曼彻斯特，或从字符波形读回比特。
 * 约定与王道一致：曼彻斯特高到低是 1；差分曼彻斯特开头不跳变是 1。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "wave.h"

static int fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

static int blank_line(const char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') {
        line++;
    }
    return *line == '\0';
}

static int cell_ok(const char *tok) {
    return (tok[0] == '-' || tok[0] == '_') && (tok[1] == '-' || tok[1] == '_');
}

static int run_draw(void) {
    char *bits = strtok(NULL, " \t\r\n");
    char *level_text = strtok(NULL, " \t\r\n");
    char *extra = strtok(NULL, " \t\r\n");
    Level prev = LEVEL_LOW;
    char nrz[WAVE_MAX * 2 + 1];
    char man[WAVE_MAX * 2 + 1];
    char diff[WAVE_MAX * 2 + 1];

    if (bits == NULL || extra != NULL) {
        return fail("draw 的格式见 TASK.md");
    }
    if (level_text != NULL && !wave_parse_level(level_text, &prev)) {
        return fail("前一比特末电平只能是 low 或 high");
    }
    if (!wave_encode(bits, prev, nrz, man, diff)) {
        return fail("比特串只能是 1 到 32 个 0 或 1");
    }
    wave_report_draw(stdout, bits, prev, nrz, man, diff);
    return 0;
}

static int decode_kind(const char *kind, char cells[][3], int n, Level prev, char *bits) {
    if (strcmp(kind, "nrz") == 0) {
        return wave_decode_nrz(cells, n, bits);
    }
    if (strcmp(kind, "man") == 0) {
        return wave_decode_man(cells, n, bits);
    }
    return wave_decode_diff(cells, n, prev, bits);
}

static int run_read(void) {
    char *kind = strtok(NULL, " \t\r\n");
    char cells[WAVE_MAX][3];
    char bits[WAVE_MAX + 1];
    int n = 0;
    Level prev = LEVEL_LOW;
    char *tok;

    if (kind == NULL) {
        return fail("read 的格式见 TASK.md");
    }
    if (strcmp(kind, "nrz") != 0 && strcmp(kind, "man") != 0 && strcmp(kind, "diff") != 0) {
        return fail("read 的编码只能是 nrz、man 或 diff");
    }
    if (strcmp(kind, "diff") == 0) {
        tok = strtok(NULL, " \t\r\n");
        if (tok == NULL || !wave_parse_level(tok, &prev)) {
            return fail("差分曼彻斯特要先写 low 或 high");
        }
    }
    while ((tok = strtok(NULL, " \t\r\n")) != NULL) {
        if (n >= WAVE_MAX || strlen(tok) != 2 || !cell_ok(tok)) {
            return fail("每一格必须是两个字符，只能用 - 和 _");
        }
        cells[n][0] = tok[0];
        cells[n][1] = tok[1];
        cells[n][2] = '\0';
        n++;
    }
    if (n < 1) {
        return fail("至少要有一格波形");
    }
    if (!decode_kind(kind, cells, n, prev, bits)) {
        return fail("这一格波形和这种编码对不上：NRZ 两半要相同，曼彻斯特和差分曼彻斯特正中间要跳变");
    }
    wave_report_read(stdout, kind, cells, n, prev, bits);
    return 0;
}

static int run_line(char *line) {
    char *cmd = strtok(line, " \t\r\n");

    if (cmd == NULL) {
        return 0;
    }
    if (strcmp(cmd, "draw") == 0) {
        return run_draw();
    }
    if (strcmp(cmd, "read") == 0) {
        return run_read();
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}

int main(void) {
    char line[512];
    int first = 1;

    while (1) {
        prompt("请输入命令（draw / read），Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            break;
        }
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
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
        if (run_line(line) != 0) {
            return 1;
        }
    }
    return 0;
}
