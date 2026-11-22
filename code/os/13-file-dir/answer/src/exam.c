#include "exam.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *id;
    char letter;
    const char *extra;
} ExamItem;

typedef struct {
    const char *name;
    const char *value;
} ClaimItem;

static const ExamItem EXAMS[] = {
    {"2009-30", 'A', 0},
    {"2009-31", 'B', "f2=1 f3=1"},
    {"2010-31", 'C', 0},
    {"2013-23", 'A', 0},
    {"2020-23", 'B', 0},
    {"2023-31", 'B', 0},
    {"2024-29", 'A', 0},
};

static const ClaimItem CLAIMS[] = {
    {"fcb-acl", "true"},
    {"fat-acl", "false"},
    {"close-nlink", "false"},
    {"close-always-drop", "false"},
    {"sys-one", "true"},
    {"user-same", "false"},
    {"hard-cross", "false"},
    {"soft-cross", "true"},
    {"hard-alive", "true"},
    {"soft-dangling", "true"},
    {"cwd-io", "false"},
    {"open-name", "true"},
    {"read-name", "false"},
    {"delete-parent", "false"},
    {"root-bypass", "false"},
    /*
     * 考纲：目录结构。王道从单级收到无环图。
     * 单级不许重名，也不适合多用户。两级只让不同用户重名，同一用户仍不能，而且不便于共享。
     * 树形不同目录可以同名，一个结点只有一个父目录，所以不便于共享，也不能有环。
     * 无环图允许多个目录项指向同一结点；删除只减共享计数，到 0 才真正删，仍然不许成环。
     */
    {"single-dup", "false"},
    {"single-multiuser", "false"},
    {"two-cross-name", "true"},
    {"two-same-name", "false"},
    {"two-share", "false"},
    {"tree-cross-name", "true"},
    {"tree-share", "false"},
    {"tree-cycle", "false"},
    {"dag-share", "true"},
    {"dag-cycle", "false"},
    {"dag-count", "true"},
    /* 考纲：目录实现。线性表要按名字逐项比；哈希表按名字定位，平均更快，冲突还在。 */
    {"linear-scan", "true"},
    {"hash-fast", "true"},
    {"hash-no-clash", "false"},
};

int exam_print(const char *id) {
    int i;
    int n = (int)(sizeof EXAMS / sizeof EXAMS[0]);

    if (strcmp(id, "2022-45") == 0) {
        /* 图里 doc 和 course1 同是索引结点 10，course1 在 30 号块。间接级见第 14 关的一般算法。 */
        printf("exam 2022-45 stu=course:2,doc:10 x=30 reads=2 indirect=1+2\n");
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(EXAMS[i].id, id) == 0) {
            if (EXAMS[i].extra != 0) {
                printf("exam %s answer=%c %s\n", id, EXAMS[i].letter, EXAMS[i].extra);
            } else {
                printf("exam %s answer=%c\n", id, EXAMS[i].letter);
            }
            return 0;
        }
    }
    return -1;
}

int claim_print(const char *name) {
    int i;
    int n = (int)(sizeof CLAIMS / sizeof CLAIMS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(CLAIMS[i].name, name) == 0) {
            printf("claim %s value=%s\n", CLAIMS[i].name, CLAIMS[i].value);
            return 0;
        }
    }
    return -1;
}

void card_print(void) {
    printf("card fcb=name-and-meta\n");
    printf("card inode=dirent-name-ino meta=separate\n");
    printf("card protect-password=stored-in-system\n");
    printf("card protect-cipher=encrypt-content\n");
    printf("card protect-acl=in-fcb\n");
}
