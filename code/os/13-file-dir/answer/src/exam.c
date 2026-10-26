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
