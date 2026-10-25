#include "build.h"
#include "exam.h"
#include "expr.h"
#include "prop.h"
#include "seq.h"
#include "tree.h"
#include "walk.h"
#include "wpl.h"

#include "prompt.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BiTree cur;
static int have;

static void fail(const char *what) {
    printf("fail %s\n", what);
}

static void drop_tree(void) {
    tree_init(&cur);
    have = 0;
}

static int read_maybe(const char *s, int *out) {
    if (strcmp(s, "?") == 0) {
        *out = -1;
        return 1;
    }
    char *end = NULL;
    errno = 0;
    long value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || value < -1 || value > 100000) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int read_num(const char *s, int *out) {
    int value = 0;
    if (!read_maybe(s, &value) || value < 0) {
        return 0;
    }
    *out = value;
    return 1;
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

static int need_tree(void) {
    if (!have || cur.root == 0) {
        fail("empty");
        return 0;
    }
    return 1;
}

static void say_build(int code) {
    if (code == -1) {
        fail("format");
    } else if (code == -2) {
        fail("char");
    } else if (code == -4) {
        fail("size");
    } else {
        fail("mismatch");
    }
}

static void say_payload(int code) {
    if (code == -1) {
        fail("format");
    } else if (code == -2) {
        fail("short");
    } else if (code == -3) {
        fail("size");
    } else if (code == -4) {
        fail("extra");
    } else {
        fail("format");
    }
}

static void cmd_prop(int argc, char **argv) {
    int n, n0, n1, n2;
    if (argc != 5 || !read_maybe(argv[1], &n) || !read_maybe(argv[2], &n0) || !read_maybe(argv[3], &n1) ||
        !read_maybe(argv[4], &n2)) {
        fail("format");
        return;
    }
    prop_print(n, n0, n1, n2, stdout);
}

static void cmd_range(int argc, char **argv) {
    int n = 0;
    if (argc != 2 || !read_num(argv[1], &n)) {
        fail("format");
        return;
    }
    range_print(n, stdout);
}

static void cmd_full(int argc, char **argv) {
    int h = 0;
    if (argc != 2 || !read_num(argv[1], &h)) {
        fail("format");
        return;
    }
    full_print(h, stdout);
}

static void cmd_cbt(int argc, char **argv) {
    int n = 0;
    int i = 0;
    if ((argc != 2 && argc != 3) || !read_num(argv[1], &n) || n < 1) {
        fail("format");
        return;
    }
    if (argc == 3 && !read_num(argv[2], &i)) {
        fail("format");
        return;
    }
    cbt_print(n, i, argc == 3, stdout);
}

static void cmd_most(int argc, char **argv) {
    int level = 0;
    int leaves = 0;
    if (argc != 3 || !read_num(argv[1], &level) || !read_num(argv[2], &leaves)) {
        fail("format");
        return;
    }
    most_print(level, leaves, stdout);
}

static void cmd_link(int argc, char **argv) {
    if (argc != 2) {
        fail("format");
        return;
    }
    int code = tree_from_pre(&cur, argv[1]);
    if (code < 0) {
        drop_tree();
        say_payload(code == -1 ? -2 : code == -2 ? -4 : -3);
        return;
    }
    if (cur.root == 0) {
        drop_tree();
        fail("empty");
        return;
    }
    have = 1;
    printf("link ok n=%d root=%c\n", cur.n, (char)cur.node[cur.root].data);
}

static void cmd_seq(int argc, char **argv) {
    SeqTree seq;
    if (argc < 2) {
        fail("format");
        return;
    }
    int code = seq_from_tokens(&seq, argv + 1, argc - 1);
    if (code == -4) {
        fail("char");
        return;
    }
    if (code == -1) {
        fail("size");
        return;
    }
    if (code != 0) {
        fail("hole");
        return;
    }
    if (seq_to_tree(&seq, &cur) != 0) {
        drop_tree();
        fail("size");
        return;
    }
    have = 1;
    seq_relations(&seq, stdout);
    char pre[BT_MAX];
    char inord[BT_MAX];
    char post[BT_MAX];
    tree_pre(&cur, cur.root, pre, BT_MAX);
    tree_in(&cur, cur.root, inord, BT_MAX);
    tree_post(&cur, cur.root, post, BT_MAX);
    printf("linked pre=%s in=%s post=%s\n", pre, inord, post);
}

static void cmd_rebuild(int argc, char **argv, int post) {
    if (argc != 3) {
        fail("format");
        return;
    }
    int id = post ? build_post_in(&cur, argv[1], argv[2], stdout) : build_pre_in(&cur, argv[1], argv[2], stdout);
    if (id <= 0) {
        drop_tree();
        say_build(id);
        return;
    }
    have = 1;
    printf("built n=%d root=%c\n", cur.n, (char)cur.node[cur.root].data);
}

static void cmd_prepost(int argc, char **argv) {
    BtForm forms[PREPOST_MAX];
    if (argc != 3) {
        fail("format");
        return;
    }
    int n = prepost_forms(argv[1], argv[2], forms, PREPOST_MAX);
    if (n == 0) {
        fail("mismatch");
        return;
    }
    if (n == -3) {
        fail("set");
        return;
    }
    if (n < 0) {
        say_build(n == -5 ? -4 : n);
        return;
    }
    printf("prepost pre=%s post=%s count=%d\n", argv[1], argv[2], n);
    for (int i = 0; i < n; i++) {
        printf("tree %d shape=%s in=%s\n", i + 1, forms[i].shape, forms[i].inord);
    }
    printf("unique=%d\n", n == 1 ? 1 : 0);
}

static void cmd_wpl(int argc, char **argv) {
    int sum = 0;
    if (argc < 2) {
        fail("format");
        return;
    }
    int code = wpl_eval(argc - 1, argv + 1, &sum, stdout);
    if (code < 0) {
        say_payload(code);
    }
}

static void cmd_infix(int argc, char **argv) {
    char buf[256];
    if (argc < 2) {
        fail("format");
        return;
    }
    int code = expr_eval(argc - 1, argv + 1, buf, (int)sizeof buf, stdout);
    if (code < 0) {
        say_payload(code);
    }
}

static void dispatch(int argc, char **argv) {
    if (strcmp(argv[0], "prop") == 0) {
        cmd_prop(argc, argv);
    } else if (strcmp(argv[0], "range") == 0) {
        cmd_range(argc, argv);
    } else if (strcmp(argv[0], "full") == 0) {
        cmd_full(argc, argv);
    } else if (strcmp(argv[0], "cbt") == 0) {
        cmd_cbt(argc, argv);
    } else if (strcmp(argv[0], "most") == 0) {
        cmd_most(argc, argv);
    } else if (strcmp(argv[0], "seq") == 0) {
        cmd_seq(argc, argv);
    } else if (strcmp(argv[0], "link") == 0) {
        cmd_link(argc, argv);
    } else if (strcmp(argv[0], "shape") == 0) {
        if (argc == 1 && need_tree()) {
            shape_print(&cur, stdout);
        } else if (argc != 1) {
            fail("format");
        }
    } else if (strcmp(argv[0], "walk") == 0) {
        if (argc == 1 && need_tree()) {
            walk_print(&cur, stdout);
        } else if (argc != 1) {
            fail("format");
        }
    } else if (strcmp(argv[0], "prein") == 0) {
        cmd_rebuild(argc, argv, 0);
    } else if (strcmp(argv[0], "postin") == 0) {
        cmd_rebuild(argc, argv, 1);
    } else if (strcmp(argv[0], "prepost") == 0) {
        cmd_prepost(argc, argv);
    } else if (strcmp(argv[0], "wpl") == 0) {
        cmd_wpl(argc, argv);
    } else if (strcmp(argv[0], "infix") == 0) {
        cmd_infix(argc, argv);
    } else if (strcmp(argv[0], "exam") == 0) {
        if (argc != 2) {
            fail("format");
        } else if (exam_run(argv[1], stdout) < 0) {
            printf("fail unknown exam %s\n", argv[1]);
        }
    } else {
        printf("fail unknown %s\n", argv[0]);
    }
}

int main(void) {
    char line[512];
    drop_tree();
    prompt("请输入命令（prop / range / full / cbt / most / seq / link / shape / walk / prein / postin / "
           "prepost / wpl / infix / exam，一行一条，Ctrl+D 结束）：\n");
    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t len = strlen(line);
        if (len + 1 == sizeof line && line[len - 1] != '\n') {
            int ch;
            fail("long");
            while ((ch = getchar()) != EOF && ch != '\n') {
            }
            continue;
        }
        char *word[64];
        int n = split_words(line, word, 64);
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
