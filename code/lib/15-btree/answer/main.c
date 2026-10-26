#include "cli.h"

#include "prompt.h"

#include <stdio.h>
#include <string.h>

#define WORD_MAX 80
#define LINE_MAX 4096
#define ARENA_MAX 4096

static void fail(const char *what) {
    printf("fail %s\n", what);
}

/* 括号单独成词，所以 (65 (60 62)) 和带空格的写法是同一种输入。 */
static int split_words(char *line, char **word, int cap, char *arena, int arena_cap) {
    int n = 0;
    int used = 0;
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
        if (*p == '(' || *p == ')') {
            word[n++] = *p == '(' ? "(" : ")";
            p++;
            continue;
        }
        {
            char *start = p;
            int len;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '(' &&
                   *p != ')') {
                p++;
            }
            len = (int)(p - start);
            if (used + len + 1 > arena_cap) {
                return -1;
            }
            memcpy(arena + used, start, (size_t)len);
            arena[used + len] = '\0';
            word[n++] = arena + used;
            used += len + 1;
        }
    }
    return n;
}

int main(void) {
    char line[LINE_MAX];
    prompt("请输入命令（bound / nodes / most / shapes / choice / bt / plus / cmp / load / "
           "policy / ins / del / find / show / stmt / trait，一行一条，Ctrl+D 结束）：\n");
    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        char arena[ARENA_MAX];
        char *word[WORD_MAX];
        int n;
        if (len + 1 == sizeof line && line[len - 1] != '\n') {
            int ch;
            fail("long");
            while ((ch = getchar()) != EOF && ch != '\n') {
            }
            continue;
        }
        n = split_words(line, word, WORD_MAX, arena, ARENA_MAX);
        if (n == 0) {
            continue;
        }
        if (n < 0) {
            fail("format");
            continue;
        }
        cli_dispatch(n, word);
    }
    cli_end();
    return 0;
}
