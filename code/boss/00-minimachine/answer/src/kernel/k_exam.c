#include "kernel/k_vm.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int mul_ok(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static int add_ok(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a > ULLONG_MAX - b) {
        return -1;
    }
    *out = a + b;
    return 0;
}

int k_addr_split(unsigned long long page_bytes, unsigned long long addr, unsigned long long *page,
                 unsigned long long *off) {
    int shift = 0;
    unsigned long long value = page_bytes;
    if (page == NULL || off == NULL || page_bytes < 2 || (page_bytes & (page_bytes - 1)) != 0) {
        return -1;
    }
    while (value > 1) {
        value >>= 1;
        shift++;
    }
    *page = addr >> shift;
    *off = addr & (page_bytes - 1);
    return 0;
}

int k_pte_addr(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long base,
               unsigned long long page, unsigned long long *addr) {
    unsigned long long bytes = 0;
    unsigned long long dummy_page = 0;
    unsigned long long dummy_off = 0;
    if (addr == NULL || pte_bytes == 0 || k_addr_split(page_bytes, 0, &dummy_page, &dummy_off) != 0) {
        return -1;
    }
    if (mul_ok(page, pte_bytes, &bytes) != 0 || add_ok(base, bytes, addr) != 0) {
        return -1;
    }
    return 0;
}

static int load_refs(const int *pages, int n, KAccess *refs) {
    int i;
    for (i = 0; i < n; i++) {
        refs[i].page = pages[i];
        refs[i].write = 0;
    }
    return n;
}

static int exam_computed(const char *id, char *buf, int cap) {
    KReport report;
    KAccess refs[16];
    if (strcmp(id, "2015-27") == 0) {
        static const int pages[] = {2, 0, 2, 9, 3, 4, 2, 8, 2, 4, 8, 4, 5, 7};
        int n = (int)(sizeof pages / sizeof pages[0]);
        load_refs(pages, n, refs);
        if (k_repl_run("lru", 4, NULL, 0, refs, n, &report) != 0 || report.step[n - 1].victim != 2) {
            return -1;
        }
        snprintf(buf, (size_t)cap, "exam 2015-27 answer=A victim=2");
        return 0;
    }
    if (strcmp(id, "2019-29") == 0) {
        static const int pages[] = {0, 1, 2, 7, 0, 5, 3, 5, 0, 2, 7, 6};
        int n = (int)(sizeof pages / sizeof pages[0]);
        load_refs(pages, n, refs);
        if (k_repl_run("lru", 4, NULL, 0, refs, n, &report) != 0 || report.faults != 9 || report.replaces != 5) {
            return -1;
        }
        snprintf(buf, (size_t)cap, "exam 2019-29 answer=C faults=9 replaces=5");
        return 0;
    }
    if (strcmp(id, "2025-26") == 0) {
        static const int pages[] = {0, 1, 2, 0, 5, 1, 4, 3, 0, 2, 3, 2, 0};
        KAccess warm[3] = {{0, 0}, {1, 0}, {2, 0}};
        int n = (int)(sizeof pages / sizeof pages[0]);
        load_refs(pages, n, refs);
        if (k_repl_run("lru", 3, warm, 3, refs, n, &report) != 0 || report.faults != 6) {
            return -1;
        }
        snprintf(buf, (size_t)cap, "exam 2025-26 answer=B faults=6");
        return 0;
    }
    if (strcmp(id, "2016-29") == 0) {
        static const int pages[] = {1, 3, 4, 5, 6, 0, 3, 2, 3, 2};
        int uniq[8];
        int nu = 0;
        if (k_ws(pages, 10, 6, uniq, &nu) != 0 || nu != 4 || uniq[0] != 0 || uniq[3] != 6) {
            return -1;
        }
        snprintf(buf, (size_t)cap, "exam 2016-29 answer=A set=0 2 3 6 size=4");
        return 0;
    }
    return 1;
}

static const char *exam_fixed(const char *id) {
    static const char *key[] = {"2014-30", "2016-26", "2021-28", "2025-27", "2019-14", "2012-25",
                                "2015-30", "2025-30", "2009-26", "2011-30", "2019-32", "2024-27"};
    static const char *line[] = {
        "exam 2014-30 answer=A note=only-fifo", "exam 2016-26 answer=A note=order-00-01-10-11",
        "exam 2021-28 answer=C note=page-2-frame-60", "exam 2025-27 answer=D", "exam 2019-14 answer=D",
        "exam 2012-25 answer=B", "exam 2015-30 answer=C", "exam 2025-30 answer=A note=ii-disputed",
        "exam 2009-26 answer=A", "exam 2011-30 answer=B note=compile-module", "exam 2019-32 answer=C",
        "exam 2024-27 answer=A"};
    int i;
    for (i = 0; i < (int)(sizeof key / sizeof key[0]); i++) {
        if (strcmp(key[i], id) == 0) {
            return line[i];
        }
    }
    return NULL;
}

int k_vm_exam(const char *id, char *buf, int cap) {
    int computed;
    const char *line;
    if (id == NULL || buf == NULL || cap < 16) {
        return -1;
    }
    computed = exam_computed(id, buf, cap);
    if (computed <= 0) {
        return computed;
    }
    line = exam_fixed(id);
    if (line == NULL) {
        return -1;
    }
    snprintf(buf, (size_t)cap, "%s", line);
    return 0;
}

int k_vm_claim(const char *name, char *buf, int cap) {
    static const char *keys[] = {"fault-restart", "fault-next", "fault-external", "fault-multi",
                                 "fault-multi-max", "vm-discrete", "vm-only-disk", "vm-only-ram",
                                 "vm-cap", "mmap-ipc", "mmap-virt", "mmap-phys", "mmap-page-disk"};
    static const char *vals[] = {"this-instr", "false", "false", "true", "6", "true", "false", "false",
                                 "address-structure", "true", "true", "false", "disputed"};
    int n = (int)(sizeof keys / sizeof keys[0]);
    int i;
    if (name == NULL || buf == NULL || cap < 16) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(keys[i], name) == 0) {
            snprintf(buf, (size_t)cap, "vclaim %s value=%s", name, vals[i]);
            return 0;
        }
    }
    return -1;
}

int k_origin(const char *mode, const char *kind, const char **from, const char **evict) {
    static const char *row[][4] = {
        {"rich", "clean", "swap", "discard"}, {"rich", "dirty", "swap", "swap"},
        {"poor", "clean", "file", "discard"}, {"poor", "dirty-first", "file", "swap"},
        {"poor", "dirty-next", "swap", "swap"}, {"unix", "fresh", "file", "discard"},
        {"unix", "swapped", "swap", "swap"}, {"unix", "shared", "memory", "none"},
    };
    int n = (int)(sizeof row / sizeof row[0]);
    int i;
    if (mode == NULL || kind == NULL || from == NULL || evict == NULL) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (strcmp(row[i][0], mode) == 0 && strcmp(row[i][1], kind) == 0) {
            *from = row[i][2];
            *evict = row[i][3];
            return 0;
        }
    }
    return -1;
}

void k_fault_demo(FILE *out) {
    int present[4];
    int dirty[4];
    int frame[4];
    int seq[4];
    int used[2];
    unsigned tick = 0;
    int refs[] = {1, 2, 3, 1};
    int write[] = {0, 1, 0, 1};
    int i;
    if (out == NULL) {
        return;
    }
    memset(present, 0, sizeof present);
    memset(dirty, 0, sizeof dirty);
    memset(frame, 0, sizeof frame);
    memset(seq, 0, sizeof seq);
    memset(used, 0, sizeof used);
    for (i = 0; i < 4; i++) {
        int page = refs[i];
        int slot = -1;
        int j;
        int victim = -1;
        int back = 0;
        int oldest = 0;
        if (present[page]) {
            fprintf(out, "pfault op=%s page=%d result=hit frame=%d victim=- writeback=0 restart=this\n",
                    write[i] ? "write" : "read", page, frame[page]);
            if (write[i]) {
                dirty[page] = 1;
            }
            continue;
        }
        for (j = 0; j < 2; j++) {
            if (!used[j]) {
                slot = j;
                break;
            }
        }
        if (slot < 0) {
            for (j = 1; j <= 3; j++) {
                if (!present[j]) {
                    continue;
                }
                if (victim < 0 || seq[j] < oldest) {
                    victim = j;
                    oldest = seq[j];
                    slot = frame[j];
                }
            }
            back = dirty[victim];
            present[victim] = 0;
            used[slot] = 0;
        }
        used[slot] = 1;
        present[page] = 1;
        frame[page] = slot;
        dirty[page] = write[i];
        seq[page] = (int)++tick;
        if (victim < 0) {
            fprintf(out, "pfault op=%s page=%d result=fault frame=%d victim=- writeback=%d restart=this\n",
                    write[i] ? "write" : "read", page, slot, back);
        } else {
            fprintf(out, "pfault op=%s page=%d result=fault frame=%d victim=%d writeback=%d restart=this\n",
                    write[i] ? "write" : "read", page, slot, victim, back);
        }
    }
}
