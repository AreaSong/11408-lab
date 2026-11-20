#include "run.h"

#include "fact.h"
#include "geom.h"
#include "sched.h"
#include "time.h"

#include <stdio.h>
#include <string.h>

static int parse_dir(const char *text, int *dir) {
    if (strcmp(text, "up") == 0) {
        *dir = 1;
        return 0;
    }
    if (strcmp(text, "down") == 0) {
        *dir = -1;
        return 0;
    }
    if (strcmp(text, "none") == 0) {
        *dir = 0;
        return 0;
    }
    return -1;
}

static int cmd_sched(char tok[][TOK_W], int ntok) {
    int head;
    int dir;
    int has_max = 0;
    int max_track = 0;
    int req[SCHED_N];
    int n;
    int i;
    SchedOut out;
    int sweep;

    if (ntok < 6 || ntok > 5 + SCHED_N) {
        return fail("bad-command");
    }
    if (take_i(tok[2], 0, 1000000, &head) != 0) {
        return 1;
    }
    if (parse_dir(tok[3], &dir) != 0) {
        return fail("bad-command");
    }
    if (strcmp(tok[4], "none") == 0) {
        has_max = 0;
    } else if (take_i(tok[4], 0, 1000000, &max_track) != 0) {
        return 1;
    } else {
        has_max = 1;
    }
    n = ntok - 5;
    for (i = 0; i < n; i++) {
        if (take_i(tok[5 + i], 0, 1000000, &req[i]) != 0) {
            return 1;
        }
    }
    sweep = strcmp(tok[1], "scan") == 0 || strcmp(tok[1], "cscan") == 0;
    if (strcmp(tok[1], "fcfs") == 0 || strcmp(tok[1], "sstf") == 0) {
        if (dir != 0) {
            return fail("bad-command");
        }
    } else if (dir == 0 || (sweep && !has_max)) {
        return fail("bad-command");
    }
    if (sched_run(tok[1], head, dir, has_max, max_track, req, n, &out) != 0) {
        return fail("bad-number");
    }
    sched_print(tok[1], &out);
    return 0;
}

static int print_time(int (*fn)(long long, int, int, int, long long, TimeParts *), char tok[][TOK_W]) {
    int seek_us;
    int rpm;
    int sectors;
    int count;
    int ctrl;
    TimeParts parts;

    if (take_i(tok[1], 0, 100000000, &seek_us) != 0 || take_i(tok[2], 1, 1000000, &rpm) != 0 ||
        take_i(tok[3], 1, 100000, &sectors) != 0 || take_i(tok[4], 1, 10000, &count) != 0 ||
        take_i(tok[5], 0, 100000000, &ctrl) != 0) {
        return 1;
    }
    if (fn(seek_us, rpm, sectors, count, ctrl, &parts) != 0) {
        return fail("bad-div");
    }
    time_print(&parts);
    return 0;
}

static int cmd_rate(char tok[][TOK_W]) {
    int seek_us;
    int rpm;
    int bytes;
    int bps;
    int ctrl;
    TimeParts parts;

    if (take_i(tok[1], 0, 100000000, &seek_us) != 0 || take_i(tok[2], 1, 1000000, &rpm) != 0 ||
        take_i(tok[3], 0, 100000000, &bytes) != 0 || take_i(tok[4], 1, 1000000000, &bps) != 0 ||
        take_i(tok[5], 0, 100000000, &ctrl) != 0) {
        return 1;
    }
    if (time_rate(seek_us, rpm, bytes, bps, ctrl, &parts) != 0) {
        return fail("bad-div");
    }
    time_print(&parts);
    return 0;
}

static int cmd_layout(char tok[][TOK_W]) {
    int nsec;
    int gap;
    int nread;
    int proc;
    int ticks;
    int wait;
    int proc_sum;

    if (take_i(tok[1], 1, 64, &nsec) != 0 || take_i(tok[2], 0, 63, &gap) != 0 ||
        take_i(tok[3], 1, 64, &nread) != 0 || take_i(tok[4], 0, 64, &proc) != 0) {
        return 1;
    }
    if (layout_run(nsec, gap, nread, proc, &ticks, &wait, &proc_sum) != 0) {
        return fail("bad-div");
    }
    printf("ticks=%d reads=%d wait=%d proc=%d\n", ticks, nread, wait, proc_sum);
    return 0;
}

static int cmd_cap(char tok[][TOK_W]) {
    int cylinders;
    int surfaces;
    int sectors;
    int sec_bytes;
    long long bytes;

    if (take_i(tok[1], 1, 1000000, &cylinders) != 0 || take_i(tok[2], 1, 10000, &surfaces) != 0 ||
        take_i(tok[3], 1, 100000, &sectors) != 0 || take_i(tok[4], 1, 65536, &sec_bytes) != 0) {
        return 1;
    }
    bytes = disk_bytes(cylinders, surfaces, sectors, sec_bytes);
    if (bytes < 0) {
        return fail("bad-number");
    }
    printf("bytes=%lld\n", bytes);
    return 0;
}

static int cmd_chs(char tok[][TOK_W]) {
    int surfaces;
    int sectors;
    int cluster_secs;
    int cluster;
    int cyl;
    int head;
    int sector;

    if (take_i(tok[1], 1, 10000, &surfaces) != 0 || take_i(tok[2], 1, 100000, &sectors) != 0 ||
        take_i(tok[3], 1, 100000, &cluster_secs) != 0 ||
        take_i(tok[4], 0, 1000000000, &cluster) != 0) {
        return 1;
    }
    if (cluster_chs(surfaces, sectors, cluster_secs, cluster, &cyl, &head, &sector) != 0) {
        return fail("bad-div");
    }
    printf("cyl=%d head=%d sector=%d\n", cyl, head, sector);
    return 0;
}

static int cmd_csstf(char tok[][TOK_W], int ntok) {
    int surfaces;
    int sectors;
    int cluster_secs;
    int head;
    int clusters[SCHED_N];
    int order[SCHED_N];
    int n;
    int i;

    if (ntok < 6 || ntok > 5 + SCHED_N) {
        return fail("bad-command");
    }
    if (take_i(tok[1], 1, 10000, &surfaces) != 0 || take_i(tok[2], 1, 100000, &sectors) != 0 ||
        take_i(tok[3], 1, 100000, &cluster_secs) != 0 || take_i(tok[4], 0, 1000000, &head) != 0) {
        return 1;
    }
    n = ntok - 5;
    for (i = 0; i < n; i++) {
        if (take_i(tok[5 + i], 0, 1000000000, &clusters[i]) != 0) {
            return 1;
        }
    }
    if (cluster_sstf(surfaces, sectors, cluster_secs, head, clusters, n, order) != 0) {
        return fail("bad-div");
    }
    printf("order=");
    for (i = 0; i < n; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%d", order[i]);
    }
    printf("\n");
    return 0;
}

static void print_page(const FtlPage *page) {
    if (page->state == 0) {
        printf("f");
    } else if (page->state == 2) {
        printf("i");
    } else {
        printf("v,%d,%d", page->lpn, page->value);
    }
}

static void ftl_show(const Ftl *disk) {
    int bi;
    int pi;

    printf("ftl host=%d flash=%d gc=%d\n", disk->host, disk->flash, disk->gc);
    for (bi = 0; bi < disk->nblk; bi++) {
        printf("b%d erase=%d", bi, disk->erase[bi]);
        for (pi = 0; pi < disk->npg; pi++) {
            printf(" p%d=", pi);
            print_page(&disk->page[bi][pi]);
        }
        printf("\n");
    }
}

static int cmd_ftl_new(Ftl *disk, char tok[][TOK_W], int ntok) {
    int blocks;
    int pages;

    if (need(ntok, 5) != 0) {
        return 1;
    }
    if (take_i(tok[2], 1, FTL_BLKS, &blocks) != 0 || take_i(tok[3], 1, FTL_PAGES, &pages) != 0) {
        return 1;
    }
    if (strcmp(tok[4], "none") != 0 && strcmp(tok[4], "dynamic") != 0) {
        return fail("bad-command");
    }
    ftl_init(disk, blocks, pages, strcmp(tok[4], "dynamic") == 0);
    printf("ftl new blocks=%d pages=%d policy=%s\n", blocks, pages, tok[4]);
    return 0;
}

static int cmd_ftl(Ftl *disk, char tok[][TOK_W], int ntok) {
    int lpn;
    int value;

    if (strcmp(tok[1], "new") == 0) {
        return cmd_ftl_new(disk, tok, ntok);
    }
    if (!disk->ready) {
        return fail("no-ftl");
    }
    if (strcmp(tok[1], "show") == 0) {
        if (need(ntok, 2) != 0) {
            return 1;
        }
        ftl_show(disk);
        return 0;
    }
    if (strcmp(tok[1], "write") != 0 || need(ntok, 4) != 0) {
        return fail("bad-command");
    }
    if (take_i(tok[2], 0, FTL_BLKS * FTL_PAGES, &lpn) != 0 || take_i(tok[3], 0, 255, &value) != 0) {
        return 1;
    }
    {
        int rc = ftl_write(disk, lpn, value);

        if (rc == -2) {
            return fail("bad-number");
        }
        if (rc != 0) {
            return fail("no-space");
        }
    }
    printf("ftl write lpn=%d value=%d\n", lpn, value);
    return 0;
}

static int cmd_wear(char tok[][TOK_W]) {
    int blocks;
    int pages;
    int times;
    int dynamic;
    int erase[FTL_BLKS];
    int host;
    int flash;
    int i;

    if (take_i(tok[1], 1, FTL_BLKS, &blocks) != 0 || take_i(tok[2], 1, FTL_PAGES, &pages) != 0 ||
        take_i(tok[3], 1, 64, &times) != 0) {
        return 1;
    }
    if (strcmp(tok[4], "none") == 0) {
        dynamic = 0;
    } else if (strcmp(tok[4], "dynamic") == 0) {
        dynamic = 1;
    } else {
        return fail("bad-command");
    }
    if (ftl_wear(blocks, pages, times, dynamic, erase, &host, &flash) != 0) {
        return fail("bad-number");
    }
    printf("policy=%s host=%d flash=%d erase=", tok[4], host, flash);
    for (i = 0; i < blocks; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%d", erase[i]);
    }
    printf("\n");
    return 0;
}

static int cmd_stick(char tok[][TOK_W]) {
    int extra;
    int order[16];
    int n = 0;
    int stuck = 0;
    int i;

    if (take_i(tok[2], 0, 8, &extra) != 0) {
        return 1;
    }
    if (stick_run(tok[1], extra, order, &n, &stuck) != 0) {
        return fail("bad-command");
    }
    printf("algo=%s extra=%d order=", tok[1], extra);
    for (i = 0; i < n; i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%d", order[i]);
    }
    printf(" stuck=%s\n", stuck ? "yes" : "no");
    return 0;
}

static int cmd_disk(char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "sched") == 0) {
        return cmd_sched(tok, ntok);
    }
    if (strcmp(tok[0], "each") == 0) {
        return need(ntok, 6) != 0 ? 1 : print_time(time_each, tok);
    }
    if (strcmp(tok[0], "sum") == 0) {
        return need(ntok, 6) != 0 ? 1 : print_time(time_sum, tok);
    }
    if (strcmp(tok[0], "rate") == 0) {
        return need(ntok, 6) != 0 ? 1 : cmd_rate(tok);
    }
    if (strcmp(tok[0], "layout") == 0) {
        return need(ntok, 5) != 0 ? 1 : cmd_layout(tok);
    }
    if (strcmp(tok[0], "cap") == 0) {
        return need(ntok, 5) != 0 ? 1 : cmd_cap(tok);
    }
    if (strcmp(tok[0], "chs") == 0) {
        return need(ntok, 5) != 0 ? 1 : cmd_chs(tok);
    }
    if (strcmp(tok[0], "csstf") == 0) {
        return cmd_csstf(tok, ntok);
    }
    return -1;
}

static int cmd_meta(Ftl *ftl, char tok[][TOK_W], int ntok) {
    if (strcmp(tok[0], "ftl") == 0) {
        return ntok < 2 ? fail("bad-command") : cmd_ftl(ftl, tok, ntok);
    }
    if (strcmp(tok[0], "wear") == 0) {
        return need(ntok, 5) != 0 ? 1 : cmd_wear(tok);
    }
    if (strcmp(tok[0], "stick") == 0) {
        return need(ntok, 3) != 0 ? 1 : cmd_stick(tok);
    }
    if (strcmp(tok[0], "exam") == 0) {
        return need(ntok, 2) != 0 ? 1 : exam_print(tok[1]) == 0 ? 0 : fail("bad-exam");
    }
    if (strcmp(tok[0], "claim") == 0) {
        return need(ntok, 2) != 0 ? 1 : claim_print(tok[1]) == 0 ? 0 : fail("bad-claim");
    }
    if (strcmp(tok[0], "card") == 0) {
        if (need(ntok, 1) != 0) {
            return 1;
        }
        card_print();
        return 0;
    }
    if (strcmp(tok[0], "fmt") == 0) {
        if (need(ntok, 1) != 0) {
            return 1;
        }
        fmt_print();
        return 0;
    }
    return -1;
}

int cmd_dispatch(Ftl *ftl, char tok[][TOK_W], int ntok) {
    int rc;

    if (ntok < 1) {
        return fail("bad-command");
    }
    rc = cmd_disk(tok, ntok);
    if (rc >= 0) {
        return rc;
    }
    rc = cmd_meta(ftl, tok, ntok);
    return rc < 0 ? fail("bad-command") : rc;
}
