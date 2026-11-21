#include "dev/dev_exam.h"

#include "dev/dev_buf.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *id;
    char letter;
} Letter;

typedef struct {
    const char *name;
    const char *value;
} Claim;

static const Letter LETTERS[] = {
    {"2009-20", 'B'}, {"2009-32", 'A'}, {"2010-24", 'C'}, {"2010-32", 'B'}, {"2011-20", 'C'},
    {"2011-22", 'C'}, {"2011-26", 'B'}, {"2012-19", 'C'}, {"2012-26", 'A'}, {"2013-25", 'C'},
    {"2014-19", 'C'}, {"2014-20", 'C'}, {"2015-19", 'C'}, {"2015-28", 'A'}, {"2016-31", 'D'},
    {"2017-32", 'B'}, {"2018-21", 'B'}, {"2019-19", 'B'}, {"2020-19", 'C'}, {"2020-25", 'D'},
    {"2020-30", 'D'}, {"2022-28", 'D'}, {"2022-32", 'A'}, {"2023-20", 'D'}, {"2023-32", 'D'},
    {"2024-20", 'B'}, {"2024-21", 'A'}, {"2024-22", 'C'}, {"2025-20", 'A'}, {"2025-21", 'B'},
};

static const Claim CLAIMS[] = {
    {"block-yields", "true"},
    {"nonblock-is-async", "false"},
    {"async-returns-before-data", "true"},
    {"duplex-needs-two", "true"},
    {"double-always-faster", "false"},
    {"alloc-creates-process", "false"},
    {"spool-user-moves", "false"},
    {"spool-needs-disk", "true"},
    {"spool-needs-multi", "true"},
    {"logic-name", "true"},
    {"driver-ignores-method", "false"},
    {"buffer-cuts-io", "true"},
    {"buffer-for-seek", "false"},
    {"pc-by-os", "false"},
    {"mask-by-os", "true"},
    {"keyboard-by-isr", "true"},
    {"cylinder-by-driver", "true"},
    {"dma-before-cpu", "true"},
};

static int put_letter(char letter, char *dst, int cap) {
    if (dst == NULL || cap < 2) {
        return -1;
    }
    dst[0] = letter;
    dst[1] = '\0';
    return 0;
}

static int exam_pair(const char *id, char letter, int n, long long t, long long m, long long c,
                     long long want_s, long long want_d, char *dst, int lcap, char *extra, int ecap) {
    long long single = dev_buf_closed(0, n, t, m, c);
    long long dual = dev_buf_closed(1, n, t, m, c);
    if (single != dev_buf_finish(0, n, t, m, c, NULL) || dual != dev_buf_finish(1, n, t, m, c, NULL)) {
        return -1;
    }
    if (single != want_s || dual != want_d || extra == NULL || ecap < 8) {
        return -1;
    }
    if (put_letter(letter, dst, lcap) != 0) {
        return -1;
    }
    snprintf(extra, (size_t)ecap, "single=%lld double=%lld", single, dual);
    (void)id;
    return 0;
}

int dev_iexam(const char *id, char *letter, int lcap, char *extra, int ecap) {
    int i;
    int n = (int)(sizeof LETTERS / sizeof LETTERS[0]);
    if (id == NULL || extra == NULL || ecap < 1) {
        return -1;
    }
    extra[0] = '\0';
    if (strcmp(id, "2011-31") == 0) {
        return exam_pair(id, 'B', 10, 100, 50, 50, 1550, 1100, letter, lcap, extra, ecap);
    }
    if (strcmp(id, "2013-27") == 0) {
        return exam_pair(id, 'C', 2, 100, 5, 90, 300, 295, letter, lcap, extra, ecap);
    }
    for (i = 0; i < n; i++) {
        if (strcmp(LETTERS[i].id, id) == 0) {
            if (put_letter(LETTERS[i].letter, letter, lcap) != 0) {
                return -1;
            }
            if (strcmp(id, "2017-32") == 0) {
                snprintf(extra, (size_t)ecap, "%s", "2,3,1,4");
            }
            return 0;
        }
    }
    return -1;
}

int dev_iclaim(const char *name, char *value, int cap) {
    int i;
    int n = (int)(sizeof CLAIMS / sizeof CLAIMS[0]);
    if (name == NULL || value == NULL || cap < 8) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(CLAIMS[i].name, name) == 0) {
            snprintf(value, (size_t)cap, "%s", CLAIMS[i].value);
            return 0;
        }
    }
    return -1;
}
