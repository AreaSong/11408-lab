#include "common/mm_shell_stage.h"

#include "common/mm_internal.h"
#include "kernel/k_buddy.h"
#include "kernel/k_proc.h"
#include "kernel/k_vm.h"

#include <stdio.h>
#include <string.h>

int mm_shell_page(char *line, FILE *out);

static KBuddy g_buddy;
static int g_buddy_on;

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest_of(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int take_word(const char **p, char *buf, int cap) {
    int n = 0;
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
    if (**p == '\0') {
        return 0;
    }
    while (**p != '\0' && **p != ' ' && **p != '\t' && n + 1 < cap) {
        buf[n++] = *(*p)++;
    }
    buf[n] = '\0';
    return 1;
}

static int take_num(const char **p, int64_t *out) {
    char word[32];
    char err[32];
    if (!take_word(p, word, (int)sizeof word)) {
        return 0;
    }
    if (mm_parse_i64(word, out, err, sizeof err) != 0) {
        return -1;
    }
    return 1;
}

static int cmd_buddy(const char *arg, FILE *out) {
    int cap = 0;
    if (sscanf(arg, "%d", &cap) != 1) {
        fprintf(out, "错误：buddy 需要容量\n");
        return -1;
    }
    if (g_buddy_on) {
        k_buddy_close(&g_buddy);
        g_buddy_on = 0;
    }
    if (k_buddy_open(&g_buddy, cap) != 0) {
        fprintf(out, "错误：容量要是 1 到 64 的 2 的幂\n");
        return -1;
    }
    g_buddy_on = 1;
    fprintf(out, "buddy mem=%d\n", cap);
    return 1;
}

static int cmd_balloc(const char *arg, FILE *out) {
    char name[16];
    int need = 0;
    int at = 0;
    int size = 0;
    int rc;
    if (!g_buddy_on || sscanf(arg, "%15s %d", name, &need) != 2) {
        fprintf(out, "错误：balloc 需要先 buddy，再写名字和大小\n");
        return -1;
    }
    rc = k_buddy_alloc(&g_buddy, name, need, &at, &size);
    if (rc == K_BUDDY_TOO_BIG) {
        fprintf(out, "balloc %s need=%d result=fail reason=too-big\n", name, need);
        return 1;
    }
    if (rc == K_BUDDY_NO_BLOCK) {
        fprintf(out, "balloc %s need=%d result=fail reason=no-block\n", name, need);
        return 1;
    }
    if (rc != 0) {
        fprintf(out, "错误：balloc 失败\n");
        return -1;
    }
    fprintf(out, "balloc %s need=%d result=ok size=%d at=%d\n", name, need, size, at);
    return 1;
}

static int cmd_bfree(const char *arg, FILE *out) {
    char name[16];
    int merges = 0;
    if (!g_buddy_on || sscanf(arg, "%15s", name) != 1 || k_buddy_free(&g_buddy, name, &merges) != 0) {
        fprintf(out, "错误：bfree 找不到这个名字\n");
        return -1;
    }
    fprintf(out, "bfree %s merges=%d\n", name, merges);
    return 1;
}

static int cmd_bshow(FILE *out) {
    KBuddyBlk blk[64];
    int n;
    int i;
    if (!g_buddy_on) {
        fprintf(out, "错误：还没有 buddy\n");
        return -1;
    }
    n = k_buddy_list(&g_buddy, 1, blk, 64);
    fprintf(out, "bshow free=%d\n", n);
    for (i = 0; i < n; i++) {
        fprintf(out, "%d %d\n", blk[i].start, blk[i].size);
    }
    n = k_buddy_list(&g_buddy, 0, blk, 64);
    fprintf(out, "jobs=%d\n", n);
    for (i = 0; i < n; i++) {
        fprintf(out, "%s %d %d\n", blk[i].name, blk[i].start, blk[i].size);
    }
    return 1;
}

static int cmd_demand(const char *arg, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    char mode[8];
    char algo[8];
    int frames = 4;
    if (sscanf(arg, "%7s", mode) != 1 || (strcmp(mode, "on") != 0 && strcmp(mode, "off") != 0)) {
        fprintf(out, "错误：demand 只能是 on 或 off\n");
        return -1;
    }
    if (strcmp(mode, "off") == 0) {
        opt->demand = 0;
        if (*machine != NULL) {
            k_demand_off(*machine);
        }
        fprintf(out, "demand off\n");
        return 1;
    }
    if (!opt->proc) {
        fprintf(out, "错误：请先 proc on\n");
        return -1;
    }
    snprintf(algo, sizeof algo, "fifo");
    sscanf(arg, "%7s %d %7s", mode, &frames, algo);
    if (k_repl_id(algo) < 0 || strcmp(algo, "opt") == 0 || frames < 1 || frames > 8) {
        fprintf(out, "错误：demand 的页框是 1 到 8，算法不能是 opt\n");
        return -1;
    }
    opt->demand = 1;
    opt->demand_frames = frames;
    snprintf(opt->demand_algo, sizeof opt->demand_algo, "%s", algo);
    if (*machine != NULL && k_proc_live(*machine) && k_demand_on(*machine, frames, algo) != 0) {
        fprintf(out, "错误：请求分页没有装上\n");
        return -1;
    }
    fprintf(out, "demand on frames=%d algo=%s\n", frames, algo);
    return 1;
}

static int vm_front(char *line, FILE *out) {
    if (starts(line, "buddy")) {
        return cmd_buddy(rest_of(line, "buddy"), out);
    }
    if (starts(line, "balloc")) {
        return cmd_balloc(rest_of(line, "balloc"), out);
    }
    if (starts(line, "bfree")) {
        return cmd_bfree(rest_of(line, "bfree"), out);
    }
    if (strcmp(line, "bshow") == 0) {
        return cmd_bshow(out);
    }
    return mm_shell_page(line, out);
}

static int cmd_vsplit(char *line, FILE *out) {
    const char *p = starts(line, "vsplit") ? rest_of(line, "vsplit") : rest_of(line, "vpte");
    int64_t a = 0;
    int64_t b = 0;
    int64_t c = 0;
    int64_t d = 0;
    unsigned long long page = 0;
    unsigned long long off = 0;
    unsigned long long addr = 0;
    if (starts(line, "vsplit")) {
        if (take_num(&p, &a) != 1 || take_num(&p, &b) != 1 ||
            k_addr_split((unsigned long long)a, (unsigned long long)b, &page, &off) != 0) {
            fprintf(out, "错误：vsplit 需要页字节和地址\n");
            return -1;
        }
        fprintf(out, "split page-bytes=0x%llx addr=0x%llx page=0x%llx offset=0x%llx\n", (unsigned long long)a,
                (unsigned long long)b, page, off);
        return 1;
    }
    if (take_num(&p, &a) != 1 || take_num(&p, &b) != 1 || take_num(&p, &c) != 1 || take_num(&p, &d) != 1 ||
        k_pte_addr((unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d,
                   &addr) != 0) {
        fprintf(out, "错误：vpte 需要页字节、项字节、表始址、页号\n");
        return -1;
    }
    fprintf(out, "pte base=0x%llx page=0x%llx addr=0x%llx\n", (unsigned long long)c, (unsigned long long)d, addr);
    return 1;
}

static int vm_note(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    char buf[160];
    const char *from = NULL;
    const char *evict = NULL;
    if (starts(line, "origin")) {
        char mode[16];
        char kind[16];
        if (sscanf(rest_of(line, "origin"), "%15s %15s", mode, kind) != 2 ||
            k_origin(mode, kind, &from, &evict) != 0) {
            fprintf(out, "错误：origin 需要来源和种类\n");
            return -1;
        }
        fprintf(out, "origin mode=%s kind=%s from=%s evict=%s\n", mode, kind, from, evict);
        return 1;
    }
    if (starts(line, "combo")) {
        char name[16];
        int id;
        if (sscanf(rest_of(line, "combo"), "%15s", name) != 1) {
            fprintf(out, "错误：combo 需要策略名\n");
            return -1;
        }
        id = k_alloc_id(name);
        if (id == -1) {
            fprintf(out, "错误：不认识的策略\n");
            return -1;
        }
        fprintf(out, "combo %s legal=%s\n", name, id > 0 ? "yes" : "no");
        return 1;
    }
    if (starts(line, "vclaim")) {
        if (k_vm_claim(rest_of(line, "vclaim"), buf, (int)sizeof buf) != 0) {
            fprintf(out, "错误：不认识的判断\n");
            return -1;
        }
        fprintf(out, "%s\n", buf);
        return 1;
    }
    if (starts(line, "exam")) {
        if (k_vm_exam(rest_of(line, "exam"), buf, (int)sizeof buf) != 0) {
            fprintf(out, "错误：不认识的题号\n");
            return -1;
        }
        fprintf(out, "%s\n", buf);
        return 1;
    }
    if (starts(line, "vsplit") || starts(line, "vpte")) {
        return cmd_vsplit(line, out);
    }
    if (strcmp(line, "pfault") == 0) {
        k_fault_demo(out);
        return 1;
    }
    if (strcmp(line, "fields") == 0) {
        fprintf(out, "fields P=存在 W=可写 U=用户 A=访问 D=修改 P0=外存地址\n");
        return 1;
    }
    if (starts(line, "demand")) {
        return cmd_demand(rest_of(line, "demand"), out, machine, opt);
    }
    return 0;
}

int mm_shell_vm(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    int front = vm_front(line, out);
    if (front != 0) {
        return front;
    }
    return vm_note(line, out, machine, opt);
}
