#include "disk.h"

#include "unit.h"

#include <stdio.h>

void disk_sector_time(double rpm, double seek_s, double sectors, DiskTime *out) {
    out->rev_s = 60.0 / rpm;
    out->seek_s = seek_s;
    out->rot_s = out->rev_s / 2.0;
    out->xfer_s = out->rev_s / sectors;
    out->ctrl_s = 0;
    out->total_s = out->seek_s + out->rot_s + out->xfer_s;
}

void disk_read_time(double rpm, double seek_s, double bytes, double rate, double ctrl_s, DiskTime *out) {
    out->rev_s = 60.0 / rpm;
    out->seek_s = seek_s;
    out->rot_s = out->rev_s / 2.0;
    out->xfer_s = bytes / rate;
    out->ctrl_s = ctrl_s;
    out->total_s = out->seek_s + out->rot_s + out->xfer_s + out->ctrl_s;
}

uint64_t disk_capacity(uint64_t surfaces, uint64_t tracks, uint64_t sectors, uint64_t sector_bytes) {
    return surfaces * tracks * sectors * sector_bytes;
}

double disk_media_rate(double rpm, uint64_t sectors, uint64_t sector_bytes) {
    return rpm / 60.0 * (double)sectors * (double)sector_bytes;
}

static void print_ms(FILE *out, const char *label, double seconds) {
    char ms[64];
    fmt_time_unit(ms, sizeof ms, seconds, "ms");
    fprintf(out, "%s = %s\n", label, ms);
}

int show_sector(FILE *out, double rpm, double seek_s, double sectors) {
    DiskTime time;
    char rev[64];
    char total[64];
    if (!(rpm > 0) || !(seek_s >= 0) || !(sectors > 0)) {
        fprintf(stderr, "sector 的转速、寻道时间或扇区数不合法\n");
        return 1;
    }
    disk_sector_time(rpm, seek_s, sectors, &time);
    fmt_time_unit(rev, sizeof rev, time.rev_s, "ms");
    fmt_time_unit(total, sizeof total, time.total_s, "ms");
    fprintf(out, "【访问一个扇区】转速 %.0f 转/分，每道 %.0f 个扇区\n", rpm, sectors);
    fprintf(out, "转一圈 = 60 / 转速 = %s。平均旋转延迟只取半圈\n", rev);
    print_ms(out, "平均旋转延迟", time.rot_s);
    print_ms(out, "传输一个扇区 = 转一圈 / 每道扇区数", time.xfer_s);
    print_ms(out, "平均寻道", time.seek_s);
    fprintf(out, "平均存取时间 = 寻道 + 旋转延迟 + 传输\n");
    fprintf(out, "结果：平均存取时间 = %s\n", total);
    return 0;
}

int show_read(FILE *out, double rpm, double seek_s, double rate, double bytes, double ctrl_s,
              double decimal_bytes) {
    DiskTime time;
    DiskTime decimal;
    char bytes_text[64], rate_text[64], xfer[64], total[64], alt[64];
    if (!(rpm > 0) || !(seek_s >= 0) || !(rate > 0) || !(bytes > 0) || !(ctrl_s >= 0)) {
        fprintf(stderr, "read 的转速、时间、传输率或数据量不合法\n");
        return 1;
    }
    disk_read_time(rpm, seek_s, bytes, rate, ctrl_s, &time);
    disk_read_time(rpm, seek_s, decimal_bytes, rate, ctrl_s, &decimal);
    fmt_num(bytes_text, sizeof bytes_text, bytes);
    fmt_num(rate_text, sizeof rate_text, rate);
    fmt_time_unit(xfer, sizeof xfer, time.xfer_s, "ms");
    fmt_time_unit(total, sizeof total, time.total_s, "ms");
    fmt_time_unit(alt, sizeof alt, decimal.total_s, "ms");
    fprintf(out, "【读一块数据】转速 %.0f 转/分\n", rpm);
    fprintf(out, "转一圈 = 60 / 转速。平均旋转延迟取半圈\n");
    print_ms(out, "平均旋转延迟", time.rot_s);
    print_ms(out, "平均寻道", time.seek_s);
    fprintf(out, "数据量按存储单位 = %s B（K/M/G = 2^10/2^20/2^30）\n", bytes_text);
    fprintf(out, "传输率按性能单位 = %s B/s（K/M/G = 10^3/10^6/10^9）\n", rate_text);
    fprintf(out, "传输时间 = 数据量 / 传输率 = %s\n", xfer);
    print_ms(out, "控制器延迟", time.ctrl_s);
    fprintf(out, "平均存取时间 = 寻道 + 旋转延迟 + 传输 + 控制器延迟 = %s\n", total);
    if (decimal_bytes != bytes) {
        fprintf(out, "若数据量里的 K/M/G 也按 10 的幂，存取时间 = %s\n", alt);
    }
    fprintf(out, "结果：平均存取时间 = %s\n", total);
    return 0;
}

int show_cap(FILE *out, uint64_t surfaces, uint64_t tracks, uint64_t sectors, uint64_t sector_bytes) {
    uint64_t bytes;
    char text[96];
    if (surfaces == 0 || tracks == 0 || sectors == 0 || sector_bytes == 0) {
        fprintf(stderr, "cap 的面数、磁道、扇区或扇区字节要是正整数\n");
        return 1;
    }
    if (tracks > UINT64_MAX / surfaces || sectors > UINT64_MAX / (surfaces * tracks) ||
        sector_bytes > UINT64_MAX / (surfaces * tracks * sectors)) {
        fprintf(stderr, "cap 的容量乘起来溢出了\n");
        return 1;
    }
    bytes = disk_capacity(surfaces, tracks, sectors, sector_bytes);
    fmt_bin_count(text, sizeof text, bytes);
    fprintf(out, "【磁盘容量】%llu 个盘面，每面 %llu 道，每道 %llu 扇区，每扇区 %llu B\n",
            (unsigned long long)surfaces, (unsigned long long)tracks, (unsigned long long)sectors,
            (unsigned long long)sector_bytes);
    fprintf(out, "柱面数 = 每面的磁道数，磁头数 = 盘面数\n");
    fprintf(out, "格式化容量 = 盘面 × 磁道 × 扇区 × 扇区字节 = %s 字节\n", text);
    fprintf(out, "非格式化容量还要把道间、扇区间隙算进去，题目给扇区大小时用的是格式化容量\n");
    fprintf(out, "结果：容量 = %s 字节\n", text);
    return 0;
}

int show_media(FILE *out, double rpm, uint64_t sectors, uint64_t sector_bytes) {
    double rate;
    char text[64];
    if (!(rpm > 0) || sectors == 0 || sector_bytes == 0) {
        fprintf(stderr, "rate 的转速或扇区参数不合法\n");
        return 1;
    }
    rate = disk_media_rate(rpm, sectors, sector_bytes);
    fmt_num(text, sizeof text, rate / 1e6);
    fprintf(out, "【内部传输率】转速 %.0f 转/分，每道 %llu 个扇区，每扇区 %llu B\n", rpm,
            (unsigned long long)sectors, (unsigned long long)sector_bytes);
    fprintf(out, "磁头不寻道、连续读一道时，传输率 = 每秒转数 × 每道字节数\n");
    fprintf(out, "这里的 M 是 10^6。它不含寻道和平均旋转延迟\n");
    fprintf(out, "结果：内部传输率 = %s MB/s\n", text);
    return 0;
}
