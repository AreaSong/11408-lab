#include "card.h"
#include "exam.h"
#include "pc.h"
#include "phi.h"
#include "prompt.h"
#include "rw.h"
#include "smoke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *text) {
    size_t n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r')) {
        text[--n] = '\0';
    }
}

static int blank(const char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

static int split(char *line, char **tok, int max) {
    int n = 0;
    char *p = line;
    while (*p != '\0' && n < max) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        tok[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return n;
}

static int parse_int(const char *text, int lo, int hi, int *out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < lo || value > hi) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

static int need(int ntok, int exact) {
    if (ntok != exact) {
        printf("bad-command\n");
        return -1;
    }
    return 0;
}

static int cmd_pc(char **tok, int ntok) {
    Scene out;
    int cap;
    int items;
    if (need(ntok, 3) != 0 || parse_int(tok[1], 1, 4, &cap) != 0 ||
        parse_int(tok[2], 1, 6, &items) != 0) {
        if (ntok == 3) {
            printf("bad-range\n");
        }
        return -1;
    }
    return scene_pc(cap, items, &out);
}

static int cmd_phi(char **tok, int ntok) {
    Scene out;
    int n;
    if (need(ntok, 3) != 0 || parse_int(tok[2], 2, 6, &n) != 0) {
        if (ntok == 3) {
            printf("bad-range\n");
        }
        return -1;
    }
    return scene_phi(tok[1], n, &out);
}

static int cmd_smoke(char **tok, int ntok) {
    Scene out;
    int code[6];
    int i;
    if (ntok < 2 || ntok > 7) {
        printf("bad-command\n");
        return -1;
    }
    for (i = 1; i < ntok; i++) {
        if (parse_int(tok[i], 1, 3, &code[i - 1]) != 0) {
            printf("bad-range\n");
            return -1;
        }
    }
    return scene_smoke(code, ntok - 1, &out);
}

static int cmd_exam(char **tok, int ntok) {
    Scene out;
    int n;
    int m;
    int trees;
    if (ntok < 2) {
        printf("bad-command\n");
        return -1;
    }
    if (strcmp(tok[1], "2019") == 0) {
        if (need(ntok, 4) != 0 || parse_int(tok[2], 3, 6, &n) != 0 ||
            parse_int(tok[3], 1, 8, &m) != 0) {
            if (ntok == 4) {
                printf("bad-range\n");
            }
            return -1;
        }
        return scene_2019(n, m, &out);
    }
    if (strcmp(tok[1], "2022") == 0) {
        return need(ntok, 2) != 0 ? -1 : scene_2022(&out);
    }
    if (strcmp(tok[1], "2024") == 0) {
        return need(ntok, 2) != 0 ? -1 : scene_2024(&out);
    }
    if (strcmp(tok[1], "2025") == 0) {
        if (need(ntok, 3) != 0 || parse_int(tok[2], 1, 6, &trees) != 0) {
            if (ntok == 3) {
                printf("bad-range\n");
            }
            return -1;
        }
        return scene_2025(trees, &out);
    }
    printf("%s unknown\n", tok[1]);
    return -1;
}

static int dispatch(char **tok, int ntok) {
    Scene out;
    if (strcmp(tok[0], "pc") == 0) {
        return cmd_pc(tok, ntok);
    }
    if (strcmp(tok[0], "order") == 0) {
        return need(ntok, 2) != 0 ? -1 : scene_order(tok[1], &out);
    }
    if (strcmp(tok[0], "rw") == 0) {
        return need(ntok, 2) != 0 ? -1 : scene_rw(tok[1], &out);
    }
    if (strcmp(tok[0], "phi") == 0) {
        return cmd_phi(tok, ntok);
    }
    if (strcmp(tok[0], "smoke") == 0) {
        return cmd_smoke(tok, ntok);
    }
    if (strcmp(tok[0], "smoke-split") == 0) {
        return need(ntok, 1) != 0 ? -1 : scene_smoke_split(&out);
    }
    if (strcmp(tok[0], "exam") == 0) {
        return cmd_exam(tok, ntok);
    }
    if (strcmp(tok[0], "card") == 0) {
        if (need(ntok, 1) != 0) {
            return -1;
        }
        scene_card();
        return 0;
    }
    printf("bad-command\n");
    return -1;
}

int main(void) {
    char line[256];
    for (;;) {
        char *tok[8];
        int ntok;
        prompt("%s", "请输入命令，Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;
        }
        trim(line);
        if (blank(line)) {
            continue;
        }
        ntok = split(line, tok, 8);
        if (ntok == 0) {
            continue;
        }
        if (dispatch(tok, ntok) != 0) {
            return 1;
        }
    }
}
