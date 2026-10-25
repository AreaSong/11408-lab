#include "exam.h"
#include "forest.h"
#include "formula.h"
#include "thr.h"

#include "prompt.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static void say(int code) {
    if (code == THR_SHORT) {
        fail("short");
    } else if (code == THR_EXTRA) {
        fail("extra");
    } else if (code == THR_SIZE) {
        fail("size");
    } else if (code == THR_EMPTY) {
        fail("empty");
    } else if (code == THR_DUP) {
        fail("dup");
    } else if (code == THR_MISS) {
        fail("missing");
    } else if (code == THR_MISMATCH) {
        fail("mismatch");
    } else if (code == THR_RANGE) {
        fail("range");
    } else {
        fail("format");
    }
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

static int read_int(const char *s, int *out, int lo, int hi) {
    char *end = NULL;
    long value;
    errno = 0;
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || value < lo || value > hi) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int one_char(const char *s) {
    if (s[0] == '\0' || s[1] != '\0') {
        return 0;
    }
    return (unsigned char)s[0];
}

static int join_tail(int argc, char **argv, char *buf, int cap) {
    int n = 0;
    buf[0] = '\0';
    for (int i = 1; i < argc; i++) {
        int len = (int)strlen(argv[i]);
        if (i > 1) {
            if (n + 1 >= cap) {
                return 0;
            }
            buf[n++] = ' ';
            buf[n] = '\0';
        }
        if (n + len >= cap) {
            return 0;
        }
        memcpy(buf + n, argv[i], (size_t)len + 1);
        n += len;
    }
    return 1;
}

static void cmd_thread(int argc, char **argv) {
    int ask = 0;
    int err;
    if (argc < 2 || argc > 3) {
        fail("format");
        return;
    }
    if (argc == 3) {
        ask = one_char(argv[2]);
        if (ask == 0) {
            fail("format");
            return;
        }
    }
    err = thr_show_in(stdout, argv[1], ask);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_post(int argc, char **argv) {
    int ask;
    int err;
    if (argc != 3) {
        fail("format");
        return;
    }
    ask = one_char(argv[2]);
    if (ask == 0) {
        fail("format");
        return;
    }
    err = thr_show_post(stdout, argv[1], ask);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_degree(int argc, char **argv) {
    DegItem items[20];
    int nitem = argc - 1;
    int err;
    if (nitem < 1 || nitem > 20) {
        fail("format");
        return;
    }
    for (int i = 0; i < nitem; i++) {
        char *end = NULL;
        long deg;
        long count;
        deg = strtol(argv[i + 1], &end, 10);
        if (end == argv[i + 1] || *end != ':') {
            fail("format");
            return;
        }
        count = strtol(end + 1, &end, 10);
        if (*end != '\0' || deg < 1 || deg > 20 || count < 0 || count > 1000000) {
            fail("range");
            return;
        }
        items[i].deg = (int)deg;
        items[i].count = (int)count;
    }
    err = degree_print(items, nitem, stdout);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_edges(int argc, char **argv) {
    int nodes;
    int edges;
    int err;
    if (argc != 3 || !read_int(argv[1], &nodes, 0, 1000000) || !read_int(argv[2], &edges, 0, 1000000)) {
        fail("format");
        return;
    }
    err = edges_print(nodes, edges, stdout);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_noright(int argc, char **argv) {
    int nodes;
    int leaves;
    int err;
    if (argc != 3 || !read_int(argv[1], &nodes, 0, 1000000) || !read_int(argv[2], &leaves, 0, 1000000)) {
        fail("format");
        return;
    }
    err = noright_print(nodes, leaves, stdout);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_height(int argc, char **argv) {
    int ways;
    int nodes;
    int err;
    if (argc != 3 || !read_int(argv[1], &ways, 0, 100) || !read_int(argv[2], &nodes, 0, 1000000)) {
        fail("format");
        return;
    }
    err = height_print(ways, nodes, stdout);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_forest(int argc, char **argv) {
    char spec[512];
    Forest forest;
    int err;
    if (!join_tail(argc, argv, spec, (int)sizeof spec)) {
        fail("long");
        return;
    }
    err = forest_parse(&forest, spec);
    if (err != THR_OK) {
        say(err);
        return;
    }
    forest_show(&forest, stdout);
}

static void cmd_bin(int argc, char **argv) {
    Forest forest;
    int err;
    if (argc != 3) {
        fail("format");
        return;
    }
    err = forest_from_bi(&forest, argv[1], argv[2]);
    if (err != THR_OK) {
        say(err);
        return;
    }
    forest_show(&forest, stdout);
}

static void cmd_pair(int argc, char **argv) {
    int err;
    if (argc != 2) {
        fail("format");
        return;
    }
    err = thr_show_pairs(stdout, argv[1]);
    if (err != THR_OK) {
        say(err);
    }
}

static void cmd_exam(int argc, char **argv) {
    if (argc != 2) {
        fail("format");
        return;
    }
    if (exam_run(argv[1], stdout) < 0) {
        printf("fail unknown exam %s\n", argv[1]);
    }
}

static void dispatch(int argc, char **argv) {
    if (strcmp(argv[0], "thread") == 0) {
        cmd_thread(argc, argv);
    } else if (strcmp(argv[0], "post") == 0) {
        cmd_post(argc, argv);
    } else if (strcmp(argv[0], "degree") == 0) {
        cmd_degree(argc, argv);
    } else if (strcmp(argv[0], "edges") == 0) {
        cmd_edges(argc, argv);
    } else if (strcmp(argv[0], "noright") == 0) {
        cmd_noright(argc, argv);
    } else if (strcmp(argv[0], "height") == 0) {
        cmd_height(argc, argv);
    } else if (strcmp(argv[0], "forest") == 0) {
        cmd_forest(argc, argv);
    } else if (strcmp(argv[0], "bin") == 0) {
        cmd_bin(argc, argv);
    } else if (strcmp(argv[0], "pair") == 0) {
        cmd_pair(argc, argv);
    } else if (strcmp(argv[0], "exam") == 0) {
        cmd_exam(argc, argv);
    } else {
        printf("fail unknown %s\n", argv[0]);
    }
}

int main(void) {
    char line[512];
    prompt("请输入命令（thread / post / degree / edges / noright / height / forest / bin / pair / exam，"
           "一行一条，Ctrl+D 结束）：\n");
    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        char *word[40];
        int n;
        if (len + 1 == sizeof line && line[len - 1] != '\n') {
            int ch;
            fail("long");
            while ((ch = getchar()) != EOF && ch != '\n') {
            }
            continue;
        }
        n = split_words(line, word, 40);
        if (n == 0) {
            continue;
        }
        if (n < 0) {
            fail("format");
            continue;
        }
        dispatch(n, word);
    }
    return 0;
}
