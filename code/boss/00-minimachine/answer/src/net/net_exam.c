#include "net/net_exam.h"

#include <string.h>

typedef struct {
    const char *id;
    char letter;
} Letter;

static const Letter LETTERS[] = {
    {"2009-34", 'B'}, {"2009-35", 'C'}, {"2009-36", 'A'}, {"2009-37", 'D'}, {"2011-34", 'B'},
    {"2011-35", 'B'}, {"2011-36", 'D'}, {"2012-36", 'B'}, {"2013-34", 'A'}, {"2013-38", 'B'},
    {"2014-34", 'B'}, {"2014-36", 'C'}, {"2014-37", 'B'}, {"2015-35", 'B'}, {"2015-36", 'B'},
    {"2015-37", 'A'}, {"2016-34", 'C'}, {"2016-35", 'D'}, {"2016-36", 'B'}, {"2017-34", 'D'},
    {"2017-35", 'B'}, {"2018-35", 'D'}, {"2019-35", 'B'}, {"2019-36", 'B'}, {"2020-35", 'C'},
    {"2020-36", 'D'}, {"2022-34", 'C'}, {"2023-34", 'C'}, {"2023-35", 'B'}, {"2023-36", 'C'},
    {"2024-35", 'D'}, {"2024-37", 'D'},
};

int net_lexam(const char *id, char *letter, int cap) {
    int i;
    int n = (int)(sizeof LETTERS / sizeof LETTERS[0]);
    if (id == NULL || letter == NULL || cap < 2) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(LETTERS[i].id, id) == 0) {
            letter[0] = LETTERS[i].letter;
            letter[1] = '\0';
            return 0;
        }
    }
    return -1;
}
