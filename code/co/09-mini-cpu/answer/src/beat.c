#include "beat.h"

#include <stdio.h>
#include <string.h>

static int ends_with(const char *text, const char *suffix) {
    size_t n = strlen(text);
    size_t m = strlen(suffix);
    return n >= m && strcmp(text + n - m, suffix) == 0;
}

int driver_count(const char *sig) {
    char buf[160];
    char *tok;
    int count = 0;
    if (sig == NULL || strcmp(sig, "无") == 0) {
        return 0;
    }
    snprintf(buf, sizeof buf, "%s", sig);
    tok = strtok(buf, " ");
    while (tok != NULL) {
        /* outE 是 MDR 通往数据总线的三态门，和内总线不是同一条线。 */
        if (!ends_with(tok, "outE") && ends_with(tok, "out")) {
            count++;
        }
        tok = strtok(NULL, " ");
    }
    return count;
}

int beat_print(FILE *out, const Beat *beat) {
    int drives = driver_count(beat->sig);
    fprintf(out, "%s C%d %s | %s | %s | 内总线驱动 %d 个\n", beat->stage, beat->index, beat->func,
            beat->detail, beat->sig, drives);
    if (drives > 1) {
        fprintf(out, "冲突：同一时钟周期只允许一个部件向内总线输出\n");
        return 1;
    }
    return 0;
}

int micro_print(FILE *out, const Micro *list, int n, int start) {
    int i;
    int bad = 0;
    for (i = 0; i < n; i++) {
        Beat beat;
        beat.index = start + i;
        beat.stage = list[i].stage;
        beat.func = list[i].func;
        beat.detail = list[i].detail != NULL ? list[i].detail : "—";
        beat.sig = list[i].sig;
        bad += beat_print(out, &beat);
    }
    return bad;
}
