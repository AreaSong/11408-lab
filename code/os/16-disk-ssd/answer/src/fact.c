#include "fact.h"

#include "geom.h"
#include "sched.h"
#include "time.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *value;
} Claim;

static const Claim CLAIMS[] = {
    {"scan-to-end", "true"},
    {"look-to-end", "false"},
    {"cscan-serves-return", "false"},
    {"jump-counts", "true"},
    {"sstf-may-starve", "true"},
    {"only-fcfs-unstuck", "true"},
    {"ssd-overwrite", "false"},
    {"wear-by-fs", "false"},
    {"wear-cuts-total", "false"},
    {"flash-prefers-fcfs", "true"},
    {"chs-by-driver", "true"},
    {"sched-cuts-seek", "true"},
    {"sched-cuts-rot", "false"},
    {"half-rot", "true"},
    {"tie-low-track", "true"},
    {"gc-copies-valid", "true"},
    {"static-moves-cold", "true"},
};

static void print_order(const SchedOut *out) {
    int i;

    for (i = 0; i < out->n; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%d", out->track[i]);
    }
}

static int exam_sched(const char *id, char letter, const char *algo, int head, int dir, int has_max,
                      int max_track, const int *req, int n) {
    SchedOut out;

    if (sched_run(algo, head, dir, has_max, max_track, req, n, &out) != 0) {
        return -1;
    }
    printf("exam %s answer=%c move=%d order=", id, letter, out.move);
    print_order(&out);
    printf("\n");
    return 0;
}

static int exam_2009(void) {
    static const int req[] = {35, 45, 12, 68, 110, 180, 170, 195};

    /* 题面没给端点，请求序列和 LOOK 相同。道数这道题不问。 */
    return exam_sched("2009-29", 'A', "look", 105, 1, 0, 0, req, 8);
}

static int exam_2015(void) {
    static const int req[] = {130, 42, 180, 15, 199};

    return exam_sched("2015-32", 'C', "scan", 58, 1, 1, 199, req, 5);
}

static int exam_2021(void) {
    static const int req[] = {184, 187, 176, 182, 199};

    return exam_sched("2021-26", 'C', "sstf", 184, 0, 1, 199, req, 5);
}

static int exam_2024(void) {
    static const int req[] = {300, 120, 110, 0, 160, 210, 399};

    return exam_sched("2024-32", 'C', "cscan", 200, -1, 1, 399, req, 7);
}

static int exam_2010(void) {
    static const int req[] = {50, 90, 30, 120};
    SchedOut out;
    TimeParts parts;
    char total[32];
    char round1[32];

    if (sched_run("clook", 100, 1, 0, 0, req, 4, &out) != 0) {
        return -1;
    }
    if (time_sum(170000, 6000, 100, 4, 0, &parts) != 0) {
        return -1;
    }
    fmt_ms(parts.seek + parts.rot + parts.xfer, 3, total, (int)sizeof total);
    fmt_ms(parts.seek + parts.rot + parts.xfer, 1, round1, (int)sizeof round1);
    printf("exam 2010-45 move=%d order=", out.move);
    print_order(&out);
    printf(" total-ms=%s round1=%s part3=fcfs\n", total, round1);
    return 0;
}

static int exam_2013(void) {
    TimeParts parts;
    char round1[32];

    if (time_rate(6000, 10000, 4000, 20000000, 200, &parts) != 0) {
        return -1;
    }
    fmt_ms(parts.seek + parts.rot + parts.xfer + parts.ctrl, 1, round1, (int)sizeof round1);
    printf("exam 2013-21 answer=B round1=%s subject=co\n", round1);
    return 0;
}

static int exam_2015_time(void) {
    TimeParts parts;
    char exact[32];
    char round1[32];
    long long sum;

    if (time_each(8000, 7200, 1000, 1, 0, &parts) != 0) {
        return -1;
    }
    sum = parts.seek + parts.rot + parts.xfer + parts.ctrl;
    fmt_ms(sum, 3, exact, (int)sizeof exact);
    fmt_ms(sum, 1, round1, (int)sizeof round1);
    printf("exam 2015-20 answer=B exact-ms=%s round1=%s subject=co\n", exact, round1);
    return 0;
}

static int exam_2019(void) {
    static const int clusters[] = {100260, 60005, 101660, 110560};
    int order[4];
    int cyl;
    int head;
    int sector;
    long long bytes;
    int i;

    bytes = disk_bytes(300, 10, 200, 512);
    if (cluster_sstf(10, 200, 2, 85, clusters, 4, order) != 0) {
        return -1;
    }
    if (cluster_chs(10, 200, 2, 100530, &cyl, &head, &sector) != 0) {
        return -1;
    }
    printf("exam 2019-44 bytes=%lld order=", bytes);
    for (i = 0; i < 4; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%d", order[i]);
    }
    printf(" cyl=%d head=%d sector=%d who=driver\n", cyl, head, sector);
    return 0;
}

int exam_print(const char *id) {
    if (strcmp(id, "2009-29") == 0) {
        return exam_2009();
    }
    if (strcmp(id, "2010-45") == 0) {
        return exam_2010();
    }
    if (strcmp(id, "2013-21") == 0) {
        return exam_2013();
    }
    if (strcmp(id, "2015-20") == 0) {
        return exam_2015_time();
    }
    if (strcmp(id, "2015-32") == 0) {
        return exam_2015();
    }
    if (strcmp(id, "2018-30") == 0) {
        printf("exam 2018-30 answer=A safe=fcfs\n");
        return 0;
    }
    if (strcmp(id, "2019-44") == 0) {
        return exam_2019();
    }
    if (strcmp(id, "2021-26") == 0) {
        return exam_2021();
    }
    if (strcmp(id, "2024-32") == 0) {
        return exam_2024();
    }
    if (strcmp(id, "2025-32") == 0) {
        printf("exam 2025-32 answer=B keep=block-size\n");
        return 0;
    }
    return -1;
}

int claim_print(const char *name) {
    int i;

    for (i = 0; i < (int)(sizeof CLAIMS / sizeof CLAIMS[0]); i++) {
        if (strcmp(CLAIMS[i].name, name) == 0) {
            printf("claim %s value=%s\n", CLAIMS[i].name, CLAIMS[i].value);
            return 0;
        }
    }
    return -1;
}

void card_print(void) {
    printf("parts=surface,track,sector,cylinder\n");
    printf("address=cylinder+head+sector\n");
    printf("same-cylinder=no-extra-seek\n");
}

void fmt_print(void) {
    printf("1 physical-format sector\n");
    printf("2 partition partition-table\n");
    printf("3 logical-format root\n");
}
