#include "fact.h"

#include "buf.h"

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
    {"2009-32", 'A'}, {"2010-24", 'C'}, {"2010-32", 'B'}, {"2011-26", 'B'},
    {"2012-26", 'A'}, {"2013-25", 'C'}, {"2015-28", 'A'}, {"2016-31", 'D'},
    {"2017-32", 'B'}, {"2020-25", 'D'}, {"2020-30", 'D'}, {"2022-28", 'D'},
    {"2022-32", 'A'}, {"2023-32", 'D'},
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
    {"rewrite-on-replace", "false"},
    {"driver-ignores-method", "false"},
    {"buffer-cuts-io", "true"},
    {"buffer-for-seek", "false"},
    {"pc-by-os", "false"},
    {"mask-by-os", "true"},
    {"keyboard-by-isr", "true"},
    {"cylinder-by-driver", "true"},
    {"safe-alloc-blocks", "true"},
};

int ctrl_print(const char *name) {
    if (strcmp(name, "poll") == 0) {
        printf("ctrl name=poll cpu=busy-wait unit=byte data=cpu irq=none\n");
        return 0;
    }
    if (strcmp(name, "interrupt") == 0) {
        printf("ctrl name=interrupt cpu=after-start unit=byte data=cpu irq=per-byte\n");
        return 0;
    }
    if (strcmp(name, "dma") == 0) {
        printf("ctrl name=dma cpu=start-end unit=block data=dma irq=per-block\n");
        return 0;
    }
    if (strcmp(name, "channel") == 0) {
        printf("ctrl name=channel cpu=start-end unit=group data=channel irq=per-io\n");
        return 0;
    }
    return -1;
}

int wait_print(const char *name) {
    if (strcmp(name, "block") == 0) {
        printf("wait name=block state=blocked ret=on-data yield=yes\n");
        return 0;
    }
    if (strcmp(name, "nonblock") == 0) {
        printf("wait name=nonblock state=running ret=eagain yield=no\n");
        return 0;
    }
    if (strcmp(name, "async") == 0) {
        printf("wait name=async state=running ret=on-complete yield=no\n");
        return 0;
    }
    return -1;
}

void layer_print(void) {
    printf("layer n=1 name=user duty=library,spool\n");
    printf("layer n=2 name=independent duty=map,buffer,alloc,protect\n");
    printf("layer n=3 name=driver duty=device-command\n");
    printf("layer n=4 name=interrupt duty=wake\n");
}

static int print_letter(const char *id) {
    int i;
    int n = (int)(sizeof LETTERS / sizeof LETTERS[0]);

    for (i = 0; i < n; i++) {
        if (strcmp(LETTERS[i].id, id) == 0) {
            printf("exam %s answer=%c\n", id, LETTERS[i].letter);
            if (strcmp(id, "2017-32") == 0) {
                printf("exam 2017-32 order=2,3,1,4\n");
            }
            return 0;
        }
    }
    return -1;
}

static int exam_pair(const char *id, char letter, int n, long long t, long long m, long long c,
                     long long want_s, long long want_d) {
    long long single = buf_closed(0, n, t, m, c);
    long long dual = buf_closed(1, n, t, m, c);

    if (single != buf_finish(0, n, t, m, c, 0) || dual != buf_finish(1, n, t, m, c, 0)) {
        return -2;
    }
    if (single != want_s || dual != want_d) {
        return -2;
    }
    printf("exam %s answer=%c single=%lld double=%lld\n", id, letter, single, dual);
    return 0;
}

int exam_print(const char *id) {
    if (strcmp(id, "2011-31") == 0) {
        return exam_pair(id, 'B', 10, 100, 50, 50, 1550, 1100);
    }
    if (strcmp(id, "2013-27") == 0) {
        return exam_pair(id, 'C', 2, 100, 5, 90, 300, 295);
    }
    return print_letter(id);
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
    printf("card unit=block,char\n");
    printf("card share=exclusive,shared,virtual\n");
    printf("card speed=low,medium,high\n");
    printf("card control=poll,interrupt,dma,channel\n");
    printf("card channel=byte-mux,selector,block-mux\n");
    printf("card layer=user,independent,driver,interrupt\n");
    printf("card driver=init,read,write,isr,major,minor\n");
    printf("card table=sdt,dct,coct,chct\n");
    printf("card spool=in-well,out-well,in-buf,out-buf,in-proc,out-proc\n");
    printf("card buffer=single,double,ring,pool\n");
}
