#include "exam.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *id;
    char letter;
    const char *note;
} ExamItem;

typedef struct {
    const char *name;
    const char *value;
} ClaimItem;

static const ExamItem EXAMS[] = {
    {"2019-14", 'D', 0},
    {"2012-25", 'B', 0},
    {"2015-30", 'C', 0},
    /* II「页面到磁盘块」各解析不一致。多数公开解析选 A，本关按这个字母，并标明争议。 */
    {"2025-30", 'A', "ii-disputed"},
};

static const ClaimItem CLAIMS[] = {
    {"fault-restart", "this-instr"},
    {"fault-next", "false"},
    {"fault-external", "false"},
    {"fault-multi", "true"},
    {"fault-multi-max", "6"},
    {"vm-discrete", "true"},
    {"vm-only-disk", "false"},
    {"vm-only-ram", "false"},
    {"vm-cap", "address-structure"},
    {"mmap-ipc", "true"},
    {"mmap-virt", "true"},
    {"mmap-phys", "false"},
    {"mmap-page-disk", "disputed"},
};

int exam_letter(const char *id, char *letter, const char **note) {
    int i;
    int n = (int)(sizeof EXAMS / sizeof EXAMS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(EXAMS[i].id, id) == 0) {
            *letter = EXAMS[i].letter;
            *note = EXAMS[i].note;
            return 0;
        }
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
    printf("card\n");
    printf("fault kind=fault detect=cpu-on-translate handler=os restart=this-instr\n");
    printf("multi max=6 parts=fetch+src+dst\n");
    printf("fields present=in-memory access=for-replace dirty=writeback disk=swap-or-file\n");
    printf("state on-fault=running-to-blocked io-done=blocked-to-ready\n");
    printf("alloc fixed-local=ok var-global=ok var-local=ok fixed-global=illegal\n");
    printf("fetch demand=one-page prep=neighbors-if-free wrong-prep=may-fault-more\n");
    printf("origin rich=copy-then-swap poor=clean-file dirty-later-swap\n");
    printf("origin unix=fresh-file swapped-from-swap shared-if-resident\n");
    printf("mmap where=virtual ipc=shared-file writeback=shared-only item-ii=disputed\n");
    printf("vm base=discrete max=address-structure\n");
}
