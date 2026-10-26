#include "exam.h"

#include <stdio.h>
#include <string.h>

int exam_letter(const char *id, char *letter) {
    if (strcmp(id, "2009-27") == 0) {
        *letter = 'C';
        return 0;
    }
    if (strcmp(id, "2010-29") == 0) {
        *letter = 'B';
        return 0;
    }
    if (strcmp(id, "2020-28") == 0) {
        *letter = 'D';
        return 0;
    }
    return -1;
}

int exam_print(const char *id) {
    char letter = '?';

    if (exam_letter(id, &letter) != 0) {
        return -1;
    }
    printf("exam %s answer=%c\n", id, letter);
    return 0;
}

void card_print(void) {
    printf("card\n");
    printf("paging dim=1 unit=fixed visible=no frag=internal share=page bound=page-ge-length\n");
    printf("segment dim=2 unit=logical visible=yes frag=external share=segment bound=seg-then-offset\n");
    printf("segpage dim=2 unit=page-in-seg visible=segment frag=internal share=segment\n");
    printf("visits paging=2 segment=2 segpage=3 tlb-hit=1\n");
    printf("multi-level why=top-le-one-page not=fewer-entries\n");
    printf("eat paths=tlb-hit,pt-hit,fault order=serial\n");
    printf("pdbr addr=physical process-switch=changes thread-switch=same\n");
    printf("clock fields=access,dirty\n");
}
