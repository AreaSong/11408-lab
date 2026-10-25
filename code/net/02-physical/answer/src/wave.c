#include "wave.h"

#include <string.h>

bool wave_parse_level(const char *text, Level *out) {
    if (text == NULL || out == NULL) {
        return false;
    }
    if (strcmp(text, "low") == 0) {
        *out = LEVEL_LOW;
        return true;
    }
    if (strcmp(text, "high") == 0) {
        *out = LEVEL_HIGH;
        return true;
    }
    return false;
}

static int bit_count(const char *bits) {
    int n = 0;

    if (bits == NULL || bits[0] == '\0') {
        return -1;
    }
    while (bits[n] != '\0') {
        if (bits[n] != '0' && bits[n] != '1') {
            return -1;
        }
        n++;
        if (n > WAVE_MAX) {
            return -1;
        }
    }
    return n;
}

static char level_char(Level level) {
    return level == LEVEL_HIGH ? '-' : '_';
}

static int read_level(char ch, Level *out) {
    if (ch == '-') {
        *out = LEVEL_HIGH;
        return 1;
    }
    if (ch == '_') {
        *out = LEVEL_LOW;
        return 1;
    }
    return 0;
}

static Level flip(Level level) {
    return level == LEVEL_HIGH ? LEVEL_LOW : LEVEL_HIGH;
}

bool wave_encode(const char *bits, Level prev, char *nrz, char *man, char *diff) {
    int n;
    int i;
    Level end = prev;

    n = bit_count(bits);
    if (n < 1 || nrz == NULL || man == NULL || diff == NULL) {
        return false;
    }
    for (i = 0; i < n; i++) {
        int one = bits[i] == '1';
        Level front;
        Level back;
        char flat = one ? '-' : '_';

        nrz[2 * i] = flat;
        nrz[2 * i + 1] = flat;
        /* 高到低是 1，低到高是 0。中间这一跳同时提供时钟 */
        man[2 * i] = one ? '-' : '_';
        man[2 * i + 1] = one ? '_' : '-';
        /* 1 沿用上一格末电平，0 先翻一次；后半格无条件再翻，只为了同步 */
        front = one ? end : flip(end);
        back = flip(front);
        diff[2 * i] = level_char(front);
        diff[2 * i + 1] = level_char(back);
        end = back;
    }
    nrz[2 * n] = '\0';
    man[2 * n] = '\0';
    diff[2 * n] = '\0';
    return true;
}

static int cell_levels(const char cell[3], Level *front, Level *back) {
    if (cell == NULL || cell[2] != '\0') {
        return 0;
    }
    return read_level(cell[0], front) && read_level(cell[1], back);
}

bool wave_decode_nrz(const char cells[][3], int n, char *bits) {
    int i;

    if (cells == NULL || bits == NULL || n < 1 || n > WAVE_MAX) {
        return false;
    }
    for (i = 0; i < n; i++) {
        Level front;
        Level back;

        if (!cell_levels(cells[i], &front, &back) || front != back) {
            return false;
        }
        bits[i] = front == LEVEL_HIGH ? '1' : '0';
    }
    bits[n] = '\0';
    return true;
}

bool wave_decode_man(const char cells[][3], int n, char *bits) {
    int i;

    if (cells == NULL || bits == NULL || n < 1 || n > WAVE_MAX) {
        return false;
    }
    for (i = 0; i < n; i++) {
        Level front;
        Level back;

        if (!cell_levels(cells[i], &front, &back) || front == back) {
            return false;
        }
        bits[i] = (front == LEVEL_HIGH) ? '1' : '0';
    }
    bits[n] = '\0';
    return true;
}

bool wave_decode_diff(const char cells[][3], int n, Level prev, char *bits) {
    int i;
    Level end = prev;

    if (cells == NULL || bits == NULL || n < 1 || n > WAVE_MAX) {
        return false;
    }
    for (i = 0; i < n; i++) {
        Level front;
        Level back;

        if (!cell_levels(cells[i], &front, &back) || front == back) {
            return false;
        }
        bits[i] = (front == end) ? '1' : '0';
        end = back;
    }
    bits[n] = '\0';
    return true;
}

static void print_bits_row(FILE *out, const char *bits, int n) {
    int i;

    fprintf(out, "%-5s", "bit");
    for (i = 0; i < n; i++) {
        fprintf(out, "| %c ", bits[i]);
    }
    fputs("|\n", out);
}

static void print_cell_row(FILE *out, const char *label, const char *packed, int n) {
    int i;

    fprintf(out, "%-5s", label);
    for (i = 0; i < n; i++) {
        fprintf(out, "| %c%c", packed[2 * i], packed[2 * i + 1]);
    }
    fputs("|\n", out);
}

static const char *level_name(Level level) {
    return level == LEVEL_HIGH ? "高" : "低";
}

void wave_report_draw(FILE *out, const char *bits, Level prev, const char *nrz,
                      const char *man, const char *diff) {
    int n = (int)strlen(bits);

    fputs("【编码波形】\n", out);
    fputs("电平：- 为高，_ 为低。每个比特占前后两格，竖线把比特隔开。\n", out);
    fputs("NRZ（不归零）：整格不变，高为 1，低为 0。连续相同比特时没有跳变，不能自同步。\n", out);
    fputs("曼彻斯特：正中间一定跳变。高到低（-_）是 1，低到高（_-）是 0。\n", out);
    fputs("这一跳既是时钟也是数据，所以每个比特是 2 个码元，码元速率是比特率的 2 倍。\n", out);
    fputs("差分曼彻斯特：正中间的跳变只做同步。开头不跳变是 1，跳变是 0。\n", out);
    fprintf(out, "前一比特末电平：%s。差分解码必须先有这个参考，缺省按低。\n", level_name(prev));
    fputs("两格长得一样也可以是不同的比特，因为比的是和上一格末电平，不是这一格的形状。\n", out);
    print_bits_row(out, bits, n);
    print_cell_row(out, "NRZ", nrz, n);
    print_cell_row(out, "MAN", man, n);
    print_cell_row(out, "DIFF", diff, n);
    fprintf(out, "结果：%s\n", bits);
}

void wave_report_read(FILE *out, const char *kind, const char cells[][3], int n,
                      Level prev, const char *bits) {
    int i;

    fputs("【由波形读比特】\n", out);
    if (strcmp(kind, "nrz") == 0) {
        fputs("编码：NRZ（不归零）\n", out);
        fputs("约定：两格都高是 1，两格都低是 0。中间没有用来携带数据的跳变。\n", out);
    } else if (strcmp(kind, "man") == 0) {
        fputs("编码：曼彻斯特\n", out);
        fputs("约定：高到低（-_）是 1，低到高（_-）是 0。正中间的跳变既是时钟也是数据。\n", out);
    } else {
        fputs("编码：差分曼彻斯特\n", out);
        fputs("约定：正中间的跳变忽略。这一格开头和上一格末电平相同是 1，不同是 0。\n", out);
        fprintf(out, "前一比特末电平：%s。\n", level_name(prev));
    }
    fputs("波形：", out);
    for (i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fputs(cells[i], out);
    }
    fputc('\n', out);
    print_bits_row(out, bits, n);
    fprintf(out, "结果：%s\n", bits);
}
