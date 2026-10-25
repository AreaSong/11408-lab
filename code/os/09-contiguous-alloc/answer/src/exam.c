#include "exam.h"

#include <stdio.h>
#include <string.h>

int exam_letter(const char *id, char *letter, const char **note) {
    if (note != 0) {
        *note = 0;
    }
    if (strcmp(id, "2009-26") == 0) {
        *letter = 'A';
        return 0;
    }
    if (strcmp(id, "2011-30") == 0) {
        *letter = 'B';
        /* 官方答案按模块内的逻辑地址。完整逻辑地址空间要到链接才形成。 */
        if (note != 0) {
            *note = "compile-module";
        }
        return 0;
    }
    if (strcmp(id, "2019-32") == 0) {
        *letter = 'C';
        return 0;
    }
    if (strcmp(id, "2024-27") == 0) {
        *letter = 'A';
        return 0;
    }
    return -1;
}

int exam_print(const char *id) {
    char letter = '?';
    const char *note = 0;
    if (exam_letter(id, &letter, &note) != 0) {
        return -1;
    }
    if (note != 0) {
        printf("exam %s answer=%c note=%s\n", id, letter, note);
    } else {
        printf("exam %s answer=%c\n", id, letter);
    }
    return 0;
}

void card_print(void) {
    printf("card\n");
    printf("space logical=from-0-per-process physical=real-cell\n");
    printf("load absolute=compile static=once-at-load dynamic=each-access\n");
    printf("protect base-limit=logical-lt-limit-then-add bounds=low-le-phys-le-high\n");
    printf("single internal=yes external=no jobs=1\n");
    printf("fixed internal=yes external=no jobs=many\n");
    printf("dynamic internal=no external=yes jobs=many\n");
    printf("ff order=address-asc from=head\n");
    printf("bf order=size-asc from=head tie=low-address\n");
    printf("wf order=size-desc from=head tie=low-address\n");
    printf("nf order=address-asc from=last-picked include-remainder\n");
    printf("buddy merge=equal-and-aligned\n");
    printf("compact needs=dynamic\n");
    printf("overlay who=same-job\n");
    printf("swap who=whole-process\n");
    printf("share va=may-differ pa=one\n");
}
