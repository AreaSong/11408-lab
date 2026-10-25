/*
 * Cache 模拟器的入口。地址划分、容量和访问过程在 src/ 里打印，这里只拆命令。
 */
#include "cache.h"
#include "fields.h"
#include "prompt.h"
#include "trace.h"
#include "walk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_MAX 64

static int parse_u64(const char *text, uint64_t *out) {
    char buf[64];
    char *end = NULL;
    unsigned long long value;
    size_t n;
    if (text == NULL || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    n = strlen(text);
    if (n >= sizeof buf) {
        return 0;
    }
    if (n > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        value = strtoull(text, &end, 0);
    } else if (n > 1 && (text[n - 1] == 'H' || text[n - 1] == 'h')) {
        memcpy(buf, text, n - 1);
        buf[n - 1] = '\0';
        value = strtoull(buf, &end, 16);
    } else {
        value = strtoull(text, &end, 10);
    }
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static int read_token(char *buf) {
    return scanf("%31s", buf) == 1;
}

static int parse_word_mode(const char *text, int *word) {
    if (strcmp(text, "byte") == 0) {
        *word = 0;
        return 1;
    }
    if (strcmp(text, "word") == 0) {
        *word = 1;
        return 1;
    }
    return 0;
}

static int parse_replace(const char *text, int *replace, int allow_none) {
    if (strcmp(text, "lru") == 0) {
        *replace = REPLACE_LRU;
        return 1;
    }
    if (strcmp(text, "fifo") == 0) {
        *replace = REPLACE_FIFO;
        return 1;
    }
    if (allow_none && strcmp(text, "none") == 0) {
        *replace = REPLACE_NONE;
        return 1;
    }
    return 0;
}

static int parse_write(const char *hit, const char *miss, int *write_back, int *write_alloc) {
    if (strcmp(hit, "back") == 0) {
        *write_back = 1;
    } else if (strcmp(hit, "through") == 0) {
        *write_back = 0;
    } else {
        return 0;
    }
    if (strcmp(miss, "alloc") == 0) {
        *write_alloc = 1;
    } else if (strcmp(miss, "noalloc") == 0) {
        *write_alloc = 0;
    } else {
        return 0;
    }
    return 1;
}

static int run_split(void) {
    char mode[32];
    int word = 0;
    int addr_bits = 0;
    int block = 0;
    int lines = 0;
    int ways = 0;
    Fields fields;
    prompt("请输入「byte 或 word  地址位数  块大小  行数  路数」，例如 byte 28 64 8 1：");
    if (!read_token(mode) || !parse_word_mode(mode, &word) ||
        scanf("%d %d %d %d", &addr_bits, &block, &lines, &ways) != 4) {
        fprintf(stderr, "split 需要：byte 或 word、地址位数、块大小、行数、路数\n");
        return 1;
    }
    if (fields_make(&fields, word, addr_bits, block, lines, ways) != 0) {
        return 1;
    }
    return show_fields(stdout, &fields);
}

static int run_map(void) {
    char mode[32];
    char addr_text[32];
    int word = 0;
    int block = 0;
    int lines = 0;
    int ways = 0;
    uint64_t addr = 0;
    Fields fields;
    prompt("请输入「byte 或 word  块大小  行数  路数  地址」，例如 byte 32 16 2 129：");
    if (!read_token(mode) || !parse_word_mode(mode, &word) || scanf("%d %d %d", &block, &lines, &ways) != 3 ||
        !read_token(addr_text) || !parse_u64(addr_text, &addr)) {
        fprintf(stderr, "map 需要：byte 或 word、块大小、行数、路数、地址\n");
        return 1;
    }
    if (fields_make(&fields, word, 0, block, lines, ways) != 0) {
        return 1;
    }
    return show_map(stdout, &fields, addr);
}

static int run_cap(void) {
    char hit[32];
    char repl[32];
    int addr_bits = 0;
    int lines = 0;
    int block = 0;
    int ways = 0;
    int write_back = 0;
    int replace = 0;
    Fields fields;
    prompt("请输入「地址位数  行数  块字节  路数  back 或 through  lru、fifo 或 none」，例如 28 8 64 1 through lru：");
    if (scanf("%d %d %d %d", &addr_bits, &lines, &block, &ways) != 4 || !read_token(hit) || !read_token(repl) ||
        (strcmp(hit, "back") != 0 && strcmp(hit, "through") != 0) || !parse_replace(repl, &replace, 1)) {
        fprintf(stderr, "cap 需要：地址位数、行数、块字节、路数、back 或 through、lru 或 fifo 或 none\n");
        return 1;
    }
    write_back = strcmp(hit, "back") == 0;
    if (fields_make(&fields, 0, addr_bits, block, lines, ways) != 0) {
        return 1;
    }
    return show_capacity(stdout, &fields, write_back, replace);
}

static int run_rate(void) {
    int accesses = 0;
    int misses = 0;
    prompt("请输入「访存次数  缺失次数」，例如 1000 50：");
    if (scanf("%d %d", &accesses, &misses) != 2) {
        fprintf(stderr, "rate 需要：访存次数、缺失次数\n");
        return 1;
    }
    return show_rate(stdout, accesses, misses);
}

static int trace_steps(Cache *cache, int count) {
    int i;
    for (i = 0; i < count; i++) {
        char op[32];
        char addr_text[32];
        uint64_t addr = 0;
        AccessInfo info;
        int is_write;
        if (!read_token(op) || !read_token(addr_text) || !parse_u64(addr_text, &addr) ||
            (strcmp(op, "R") != 0 && strcmp(op, "W") != 0)) {
            fprintf(stderr, "第 %d 次访问要写成 R 或 W，再跟地址\n", i + 1);
            return 1;
        }
        is_write = strcmp(op, "W") == 0;
        if (cache_access(cache, is_write, addr, &info) != 0) {
            return 1;
        }
        trace_show_access(stdout, i + 1, is_write, addr, cache, &info);
    }
    return 0;
}

static int run_trace(void) {
    char mode[32];
    char repl[32];
    char hit[32];
    char miss[32];
    int word = 0;
    int addr_bits = 0;
    int block = 0;
    int lines = 0;
    int ways = 0;
    int count = 0;
    int replace = 0;
    int write_back = 0;
    int write_alloc = 0;
    int rc;
    Fields fields;
    Cache *cache = NULL;
    prompt("请输入「byte 或 word  地址位数  块大小  行数  路数  lru 或 fifo  back 或 through  alloc 或 noalloc」，"
           "下一行是访问次数，接着每行「R 或 W  地址」：");
    if (!read_token(mode) || !parse_word_mode(mode, &word) ||
        scanf("%d %d %d %d", &addr_bits, &block, &lines, &ways) != 4 || !read_token(repl) || !read_token(hit) ||
        !read_token(miss) || !parse_replace(repl, &replace, 0) || !parse_write(hit, miss, &write_back, &write_alloc)) {
        fprintf(stderr, "trace 的参数是：byte 或 word、地址位数、块大小、行数、路数、lru 或 fifo、back 或 through、alloc 或 noalloc\n");
        return 1;
    }
    if (scanf("%d", &count) != 1 || count < 1 || count > TRACE_MAX) {
        fprintf(stderr, "访问次数要在 1 到 %d 之间\n", TRACE_MAX);
        return 1;
    }
    if (fields_make(&fields, word, addr_bits, block, lines, ways) != 0 ||
        cache_open(&cache, &fields, replace, write_back, write_alloc) != 0) {
        return 1;
    }
    trace_show_header(stdout, &fields, replace, write_back, write_alloc);
    rc = trace_steps(cache, count);
    if (rc == 0) {
        trace_show_summary(stdout, cache);
    }
    cache_close(cache);
    return rc;
}

static int run_loop(void) {
    char order_text[32];
    char op_text[32];
    char base_text[32];
    char repl[32];
    char hit[32];
    char miss[32];
    WalkIn in;
    int which = WALK_ROW;
    memset(&in, 0, sizeof in);
    prompt("请输入「row、col 或 both  read 或 update  地址位数  行数  列数  元素字节  首地址  块字节  "
           "Cache行数  路数  lru 或 fifo  back 或 through  alloc 或 noalloc」：");
    if (!read_token(order_text) || !read_token(op_text) || scanf("%d %d %d %d", &in.addr_bits, &in.rows, &in.cols, &in.elem_bytes) != 4 ||
        !read_token(base_text) || !parse_u64(base_text, &in.base) ||
        scanf("%d %d %d", &in.block_bytes, &in.n_lines, &in.n_ways) != 3 || !read_token(repl) || !read_token(hit) ||
        !read_token(miss)) {
        fprintf(stderr, "loop 的参数没读全\n");
        return 1;
    }
    if (strcmp(order_text, "row") == 0) {
        which = WALK_ROW;
    } else if (strcmp(order_text, "col") == 0) {
        which = WALK_COL;
    } else if (strcmp(order_text, "both") == 0) {
        which = WALK_BOTH;
    } else {
        fprintf(stderr, "访问顺序要是 row、col 或 both\n");
        return 1;
    }
    if (strcmp(op_text, "read") == 0) {
        in.update = 0;
    } else if (strcmp(op_text, "update") == 0) {
        in.update = 1;
    } else {
        fprintf(stderr, "读写方式要是 read 或 update\n");
        return 1;
    }
    if (!parse_replace(repl, &in.replace, 0) || !parse_write(hit, miss, &in.write_back, &in.write_alloc)) {
        fprintf(stderr, "替换用 lru 或 fifo，写策略用 back 或 through，以及 alloc 或 noalloc\n");
        return 1;
    }
    return show_walk(stdout, &in, which);
}

int main(void) {
    char cmd[32];
    for (;;) {
        int rc;
        prompt("请输入命令（split / map / cap / rate / trace / loop，Ctrl+D 结束）：");
        if (!read_token(cmd)) {
            break;
        }
        if (strcmp(cmd, "split") == 0) {
            rc = run_split();
        } else if (strcmp(cmd, "map") == 0) {
            rc = run_map();
        } else if (strcmp(cmd, "cap") == 0) {
            rc = run_cap();
        } else if (strcmp(cmd, "rate") == 0) {
            rc = run_rate();
        } else if (strcmp(cmd, "trace") == 0) {
            rc = run_trace();
        } else if (strcmp(cmd, "loop") == 0) {
            rc = run_loop();
        } else {
            fprintf(stderr, "未知命令：%s\n", cmd);
            return 1;
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
