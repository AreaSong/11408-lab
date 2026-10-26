#include "exam.h"
#include "insn.h"
#include "metric.h"
#include "pipe.h"
#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
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

static int cmd_ideal(const char *rest) {
    int k;
    int n;
    long long dt;
    char unit[16];
    if (sscanf(rest, "%d %d %lld %15s", &k, &n, &dt, unit) != 4 || k < 1 || k > 8 || n < 1 ||
        dt <= 0) {
        fprintf(stderr, "ideal 写成：ideal 段数 指令数 段时间 单位\n");
        return -1;
    }
    metric_ideal(stdout, k, n, dt, unit);
    return 0;
}

static int cmd_freq(const char *rest) {
    int k;
    int n;
    int mhz;
    char unit[16];
    if (sscanf(rest, "%d %d %d %15s", &k, &n, &mhz, unit) != 4 || k < 1 || n < 1 || mhz <= 0 ||
        strcmp(unit, "MHz") != 0) {
        fprintf(stderr, "freq 写成：freq 段数 指令数 主频 MHz\n");
        return -1;
    }
    metric_freq(stdout, k, n, mhz);
    return 0;
}

static int cmd_clock(const char *rest) {
    char buf[180];
    char *save = NULL;
    char *tok;
    long long nums[9];
    int n = 0;
    const char *unit = NULL;
    int i;
    snprintf(buf, sizeof buf, "%s", rest);
    for (tok = strtok_r(buf, " \t", &save); tok != NULL; tok = strtok_r(NULL, " \t", &save)) {
        char *end = NULL;
        long long value = strtoll(tok, &end, 10);
        if (end != tok && *end == '\0') {
            if (n >= 9) {
                fprintf(stderr, "功能段太多\n");
                return -1;
            }
            nums[n++] = value;
        } else if (unit == NULL) {
            unit = tok;
        } else {
            fprintf(stderr, "clock 的单位重复了\n");
            return -1;
        }
    }
    if (unit == NULL || n < 2) {
        fprintf(stderr, "clock 写成：clock 段时间… 锁存 单位\n");
        return -1;
    }
    for (i = 0; i < n - 1; i++) {
        if (nums[i] <= 0) {
            fprintf(stderr, "功能段时间要大于 0\n");
            return -1;
        }
    }
    if (nums[n - 1] < 0) {
        fprintf(stderr, "锁存时间不能是负的\n");
        return -1;
    }
    metric_clock(stdout, nums, n - 1, nums[n - 1], unit);
    return 0;
}

static int read_block(char *buf, int cap) {
    int used = 0;
    buf[0] = '\0';
    for (;;) {
        char line[160];
        int len;
        prompt("请输入指令：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            fprintf(stderr, "指令序列没有以 . 结束\n");
            return -1;
        }
        trim(line);
        if (strcmp(line, ".") == 0) {
            return used > 0 ? 0 : -1;
        }
        if (line[0] == '\0') {
            continue;
        }
        len = (int)strlen(line);
        if (used + len + 2 >= cap) {
            fprintf(stderr, "指令序列太长\n");
            return -1;
        }
        memcpy(buf + used, line, (size_t)len);
        used += len;
        buf[used++] = '\n';
        buf[used] = '\0';
    }
}

static int cmd_pipe(const char *mode) {
    char src[4096];
    Insn buf[INSN_N];
    PipeOpt opt;
    PipeStat st;
    char err[160];
    int n;
    if (pipe_opt_named(mode, &opt) != 0) {
        fprintf(stderr, "不认识的流水线方式：%s\n", mode);
        return -1;
    }
    if (read_block(src, (int)sizeof src) != 0) {
        fprintf(stderr, "pipe 后面要有指令，并用单独一行 . 结束\n");
        return -1;
    }
    n = insn_parse(src, buf, INSN_N, err, (int)sizeof err);
    if (n < 0) {
        fprintf(stderr, "%s\n", err);
        return -1;
    }
    if (pipe_run(buf, n, &opt, &st) != 0) {
        fprintf(stderr, "流水线没有跑完\n");
        return -1;
    }
    pipe_report(stdout, buf, n, &opt, &st);
    return 0;
}

static int dispatch(char *line) {
    const char *rest = NULL;
    if (show_exam(stdout, line) == 0) {
        return 0;
    }
    if (starts(line, "ideal ", &rest)) {
        return cmd_ideal(rest);
    }
    if (starts(line, "freq ", &rest)) {
        return cmd_freq(rest);
    }
    if (starts(line, "clock ", &rest)) {
        return cmd_clock(rest);
    }
    if (starts(line, "pipe ", &rest)) {
        return cmd_pipe(rest);
    }
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
