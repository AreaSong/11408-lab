#include "cli.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

#define WORD_MAX 40
#define LINE_MAX 4096

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static int split_words(char *line, char **word, int cap) {
    int n = 0;
    char *p = line;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p == '\0' || *p == '\n') {
            break;
        }
        if (n >= cap) {
            return -1;
        }
        word[n++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            p++;
        }
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return n;
}

int main(void) {
    char line[LINE_MAX];
    prompt("请输入命令（new / edge / show / prim / kruskal / dijkstra / floyd / "
           "topo / topo-all / topo-check / aoe，一行一条，Ctrl+D 结束）：\n");
    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        char *word[WORD_MAX];
        int n;
        if (len + 1 == sizeof line && line[len - 1] != '\n') {
            int ch;
            fail("long");
            while ((ch = getchar()) != EOF && ch != '\n') {
            }
            continue;
        }
        n = split_words(line, word, WORD_MAX);
        if (n == 0) {
            continue;
        }
        if (n < 0) {
            fail("format");
            continue;
        }
        cli_dispatch(n, word);
    }
    return 0;
}
