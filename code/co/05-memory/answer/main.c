/*
 * 存储器计算器的入口。算式和单位在 src/ 里打印，这里只拆命令。
 */
#include <stdio.h>
#include <string.h>

#include "cross.h"
#include "disk.h"
#include "dram.h"
#include "expand.h"
#include "prompt.h"
#include "unit.h"

static int read_bin(uint64_t *out) {
    double value;
    char unit[16];
    return scanf("%lf %15s", &value, unit) == 2 && parse_bin(value, unit, out);
}

static int read_seconds(double *out) {
    double value;
    char unit[16];
    return scanf("%lf %15s", &value, unit) == 2 && parse_seconds(value, unit, out);
}

static int run_dram(void) {
    double value;
    char unit[16];
    int bits;
    uint64_t cells;
    prompt("请输入「单元数 单位 数据位数」，例如 4 M 8：");
    if (scanf("%lf %15s %d", &value, unit, &bits) != 3 || !parse_bin(value, unit, &cells)) {
        fprintf(stderr, "dram 需要：单元数 单位 数据位数\n");
        return 1;
    }
    return show_dram(stdout, cells, bits);
}

static int run_refresh(void) {
    unsigned long long rows = 0;
    double interval = 0;
    double cycle = 0;
    prompt("请输入「行数 刷新间隔 单位 存取周期 单位」，例如 128 2 ms 0.5 us：");
    if (scanf("%llu", &rows) != 1 || !read_seconds(&interval) || !read_seconds(&cycle)) {
        fprintf(stderr, "refresh 需要：行数 刷新间隔 单位 存取周期 单位\n");
        return 1;
    }
    return show_refresh(stdout, (uint64_t)rows, interval, cycle);
}

static int run_expand(void) {
    uint64_t target = 0;
    uint64_t chip = 0;
    int target_bits = 0;
    int chip_bits = 0;
    prompt("请输入「目标单元数 单位 目标位宽 芯片单元数 单位 芯片位宽」，例如 8 K 8 2 K 4：");
    if (!read_bin(&target) || scanf("%d", &target_bits) != 1 || !read_bin(&chip) ||
        scanf("%d", &chip_bits) != 1) {
        fprintf(stderr, "expand 需要：目标单元数 单位 目标位宽 芯片单元数 单位 芯片位宽\n");
        return 1;
    }
    return show_expand(stdout, target, target_bits, chip, chip_bits);
}

static int run_locate(void) {
    uint64_t target = 0;
    uint64_t chip = 0;
    char text[32];
    uint64_t addr = 0;
    prompt("请输入「目标单元数 单位 每组单元数 单位 地址」，例如 8 K 2 K 0B1FH：");
    if (!read_bin(&target) || !read_bin(&chip) || scanf("%31s", text) != 1 || !parse_addr(text, &addr)) {
        fprintf(stderr, "locate 需要：目标单元数 单位 每组单元数 单位 地址（如 0B1FH）\n");
        return 1;
    }
    return show_locate(stdout, target, chip, addr);
}

static int run_mar(void) {
    uint64_t space = 0;
    prompt("请输入「地址空间 单位」，例如 64 MB：");
    if (!read_bin(&space)) {
        fprintf(stderr, "mar 需要：地址空间 单位\n");
        return 1;
    }
    return show_mar(stdout, space);
}

static int run_zone(void) {
    int bits = 0;
    int low = 0;
    int high = 0;
    prompt("请输入「地址线根数 RAM 的份数 ROM 的份数」，例如 30 3 1：");
    if (scanf("%d %d %d", &bits, &low, &high) != 3) {
        fprintf(stderr, "zone 需要：地址线根数 RAM份数 ROM份数\n");
        return 1;
    }
    return show_zone(stdout, bits, low, high);
}

static int run_cs(void) {
    int bits = 0;
    int chip_bits = 0;
    char pattern[64];
    prompt("请输入「地址线根数 片内地址位数 片选约束」，例如 16 13 001 或 16 13 0xx：");
    if (scanf("%d %d %63s", &bits, &chip_bits, pattern) != 3) {
        fprintf(stderr, "cs 需要：地址线根数 片内地址位数 片选约束\n");
        return 1;
    }
    return show_cs(stdout, bits, chip_bits, pattern);
}

static int run_bank(void) {
    int modules = 0;
    int n = 0;
    int i;
    uint64_t addrs[32];
    prompt("请输入「模块数 地址个数」，接着输入十进制地址，例如 4 9 8005 8006 …：");
    if (scanf("%d %d", &modules, &n) != 2 || n < 1 || n > 32) {
        fprintf(stderr, "bank 需要：模块数 地址个数（1 到 32），然后是这些地址\n");
        return 1;
    }
    for (i = 0; i < n; i++) {
        unsigned long long addr = 0;
        if (scanf("%llu", &addr) != 1) {
            fprintf(stderr, "第 %d 个地址没读到\n", i + 1);
            return 1;
        }
        addrs[i] = (uint64_t)addr;
    }
    return show_bank(stdout, modules, addrs, n);
}

static int run_span(void) {
    int modules = 0;
    int bus_bits = 0;
    unsigned long long nbytes = 0;
    char text[32];
    uint64_t addr = 0;
    prompt("请输入「模块数 总线位数 字节数 起始地址」，例如 4 32 8 804001AH：");
    if (scanf("%d %d %llu %31s", &modules, &bus_bits, &nbytes, text) != 4 || !parse_addr(text, &addr)) {
        fprintf(stderr, "span 需要：模块数 总线位数 字节数 起始地址\n");
        return 1;
    }
    return show_span(stdout, modules, bus_bits, (uint64_t)nbytes, addr);
}

static int run_seq(void) {
    int modules = 0;
    int words = 0;
    int word_bytes = 0;
    double cycle = 0;
    double bus = 0;
    prompt("请输入「模块数 存取周期 单位 总线周期 单位 字数 每字字节」，例如 4 200 ns 50 ns 4 4：");
    if (scanf("%d", &modules) != 1 || !read_seconds(&cycle) || !read_seconds(&bus) ||
        scanf("%d %d", &words, &word_bytes) != 2) {
        fprintf(stderr, "seq 需要：模块数 存取周期 单位 总线周期 单位 字数 每字字节\n");
        return 1;
    }
    return show_seq(stdout, modules, cycle, bus, words, word_bytes);
}

static int run_bw(void) {
    int channels = 0;
    int bits = 0;
    double value = 0;
    double hz = 0;
    char unit[16];
    prompt("请输入「通道数 频率 单位 位宽」，例如 3 1333 MHz 64：");
    if (scanf("%d %lf %15s %d", &channels, &value, unit, &bits) != 4 || !parse_hz(value, unit, &hz)) {
        fprintf(stderr, "bw 需要：通道数 频率 单位 位宽\n");
        return 1;
    }
    return show_bw(stdout, channels, hz, bits);
}

static int run_sector(void) {
    double rpm = 0;
    double seek = 0;
    double sectors = 0;
    prompt("请输入「转速 平均寻道 单位 每道扇区数」，例如 7200 8 ms 1000：");
    if (scanf("%lf", &rpm) != 1 || !read_seconds(&seek) || scanf("%lf", &sectors) != 1) {
        fprintf(stderr, "sector 需要：转速 平均寻道 单位 每道扇区数\n");
        return 1;
    }
    return show_sector(stdout, rpm, seek, sectors);
}

static int run_read(void) {
    double rpm = 0;
    double seek = 0;
    double rate_value = 0;
    double rate = 0;
    double data_value = 0;
    double ctrl = 0;
    double dec_bytes = 0;
    char rate_unit[16];
    char data_unit[16];
    uint64_t bytes = 0;
    prompt("请输入「转速 寻道 单位 传输率 单位 数据量 单位 控制器延迟 单位」，例如 10000 6 ms 20 MB/s 4 KB 0.2 ms：");
    if (scanf("%lf", &rpm) != 1 || !read_seconds(&seek) ||
        scanf("%lf %15s %lf %15s", &rate_value, rate_unit, &data_value, data_unit) != 4 ||
        !parse_rate(rate_value, rate_unit, &rate) || !parse_bin(data_value, data_unit, &bytes) ||
        !parse_dec_bytes(data_value, data_unit, &dec_bytes) || !read_seconds(&ctrl)) {
        fprintf(stderr, "read 需要：转速 寻道 单位 传输率 单位 数据量 单位 控制器延迟 单位\n");
        return 1;
    }
    return show_read(stdout, rpm, seek, rate, (double)bytes, ctrl, dec_bytes);
}

static int run_cap(void) {
    unsigned long long surfaces = 0;
    unsigned long long tracks = 0;
    unsigned long long sectors = 0;
    unsigned long long sector_bytes = 0;
    prompt("请输入「盘面数 每面磁道数 每道扇区数 每扇区字节数」，例如 2 500 32 512：");
    if (scanf("%llu %llu %llu %llu", &surfaces, &tracks, &sectors, &sector_bytes) != 4) {
        fprintf(stderr, "cap 需要：盘面数 每面磁道数 每道扇区数 每扇区字节数\n");
        return 1;
    }
    return show_cap(stdout, (uint64_t)surfaces, (uint64_t)tracks, (uint64_t)sectors,
                    (uint64_t)sector_bytes);
}

static int run_media(void) {
    double rpm = 0;
    unsigned long long sectors = 0;
    unsigned long long sector_bytes = 0;
    prompt("请输入「转速 每道扇区数 每扇区字节数」，例如 7200 1000 512：");
    if (scanf("%lf %llu %llu", &rpm, &sectors, &sector_bytes) != 3) {
        fprintf(stderr, "media 需要：转速 每道扇区数 每扇区字节数\n");
        return 1;
    }
    return show_media(stdout, rpm, (uint64_t)sectors, (uint64_t)sector_bytes);
}

int main(void) {
    char cmd[32];
    for (;;) {
        int rc;
        prompt("请输入命令（dram / refresh / expand / locate / mar / zone / cs / bank / span / seq / bw / sector / read / cap / media，Ctrl+D 结束）：");
        if (scanf("%31s", cmd) != 1) {
            break;
        }
        if (strcmp(cmd, "dram") == 0) {
            rc = run_dram();
        } else if (strcmp(cmd, "refresh") == 0) {
            rc = run_refresh();
        } else if (strcmp(cmd, "expand") == 0) {
            rc = run_expand();
        } else if (strcmp(cmd, "locate") == 0) {
            rc = run_locate();
        } else if (strcmp(cmd, "mar") == 0) {
            rc = run_mar();
        } else if (strcmp(cmd, "zone") == 0) {
            rc = run_zone();
        } else if (strcmp(cmd, "cs") == 0) {
            rc = run_cs();
        } else if (strcmp(cmd, "bank") == 0) {
            rc = run_bank();
        } else if (strcmp(cmd, "span") == 0) {
            rc = run_span();
        } else if (strcmp(cmd, "seq") == 0) {
            rc = run_seq();
        } else if (strcmp(cmd, "bw") == 0) {
            rc = run_bw();
        } else if (strcmp(cmd, "sector") == 0) {
            rc = run_sector();
        } else if (strcmp(cmd, "read") == 0) {
            rc = run_read();
        } else if (strcmp(cmd, "cap") == 0) {
            rc = run_cap();
        } else if (strcmp(cmd, "media") == 0) {
            rc = run_media();
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
