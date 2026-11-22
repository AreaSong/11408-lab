#include "card.h"
#include "exam.h"
#include "pc.h"
#include "phi.h"
#include "prec.h"
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

/* 边写成 u-v，u 是前驱，v 是后继。返回 0 成功，-1 格式不对，-2 越界或自环。 */
static int parse_edge(const char *text, int n, int *u, int *v) {
    char *end = NULL;
    char *stop = NULL;
    long from = strtol(text, &end, 10);
    long to;
    if (end == text || *end != '-') {
        return -1;
    }
    to = strtol(end + 1, &stop, 10);
    if (stop == end + 1 || *stop != '\0') {
        return -1;
    }
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) {
        return -2;
    }
    *u = (int)from;
    *v = (int)to;
    return 0;
}

static int cmd_prec(char **tok, int ntok) {
    Scene out;
    int u[PV_MAX_SEM];
    int v[PV_MAX_SEM];
    int n;
    int m;
    int i;
    int j;
    int rc;
    if (ntok < 2) {
        printf("bad-command\n");
        return -1;
    }
    if (parse_int(tok[1], 2, 6, &n) != 0) {
        printf("bad-range\n");
        return -1;
    }
    m = ntok - 2;
    if (m > PV_MAX_SEM) {
        printf("bad-range\n");
        return -1;
    }
    for (i = 0; i < m; i++) {
        rc = parse_edge(tok[i + 2], n, &u[i], &v[i]);
        if (rc == -1) {
            printf("bad-command\n");
            return -1;
        }
        if (rc == -2) {
            printf("bad-range\n");
            return -1;
        }
        for (j = 0; j < i; j++) {
            if (u[j] == u[i] && v[j] == v[i]) {
                printf("bad-range\n");
                return -1;
            }
        }
    }
    rc = scene_prec(n, u, v, m, &out);
    if (rc == -2) {
        printf("bad-range\n");
        return -1;
    }
    return rc;
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
    if (strcmp(tok[1], "2020") == 0) {
        return need(ntok, 2) != 0 ? -1 : scene_2020(&out);
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
    if (strcmp(tok[0], "prec") == 0) {
        return cmd_prec(tok, ntok);
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
        char *tok[16];
        int ntok;
        prompt("%s", "请输入命令，Ctrl+D 结束：");
        if (fgets(line, sizeof line, stdin) == NULL) {
            return 0;
        }
        trim(line);
        if (blank(line)) {
            continue;
        }
        ntok = split(line, tok, 16);
        if (ntok == 0) {
            continue;
        }
        if (dispatch(tok, ntok) != 0) {
            return 1;
        }
    }
}
