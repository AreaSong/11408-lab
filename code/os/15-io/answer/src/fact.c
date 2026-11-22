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
    /*
     * 考纲：I/O 接口与 I/O 端口。口径与计组 co/13 卡片三相同。
     * 端口是接口里 CPU 能直接访问的寄存器，不是设备本身。
     * 统一编址用访存指令，端口占掉一部分主存地址；
     * 独立编址用专用输入/输出指令，不占主存，但要另有一根信号区分访存和 I/O。
     */
    {"port-is-reg", "true"},
    {"port-is-device", "false"},
    {"unified-mem-insn", "true"},
    {"unified-io-insn", "false"},
    {"unified-occupy-mem", "true"},
    {"indep-io-insn", "true"},
    {"indep-occupy-mem", "false"},
    {"indep-need-flag", "true"},
    /*
     * 考纲：字符设备接口、块设备接口、网络设备接口。
     * 字符接口按 get/put，不能按块号寻址；块接口可以；
     * 网络接口按报文收发，既不是字符流，也不是可寻址的块。
     */
    {"char-random", "false"},
    {"block-random", "true"},
    {"net-packet", "true"},
    {"net-is-char", "false"},
    {"net-is-block", "false"},
    /*
     * 考纲：缓冲池。空缓冲、输入、输出三个队列公用缓冲区。
     * hin 从空队列取出，装满挂到输入队列；sin 从输入队列取出，用完归还空队列。
     * hout、sout 对输出队列做同样的事。四个工作缓冲正在用时不挂在队列上。
     */
    {"pool-queues", "empty,input,output"},
    {"pool-hin", "from-empty"},
    {"pool-hin-full", "to-input"},
    {"pool-sin", "from-input"},
    {"pool-sin-done", "to-empty"},
    {"pool-hout", "from-empty"},
    {"pool-hout-full", "to-output"},
    {"pool-sout", "from-output"},
    {"pool-sout-done", "to-empty"},
    {"pool-work-queued", "false"},
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
