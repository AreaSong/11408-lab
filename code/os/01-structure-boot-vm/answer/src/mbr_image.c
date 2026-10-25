#include "mbr_image.h"

#include <string.h>

/* 分区表项里的 LBA 和扇区数是小端 4 字节。CHS 三个字节本关填 0，考纲不考 CHS 编码。 */

static void write_u32_le(unsigned char *p, uint32_t value) {
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static uint32_t read_u32_le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_entry(unsigned char *entry, const PartSpec *part) {
    entry[0] = part->status;
    entry[4] = part->type;
    write_u32_le(entry + 8, part->lba);
    write_u32_le(entry + 12, part->sectors);
}

static void read_entry(const unsigned char *entry, PartInfo *info) {
    info->status = entry[0];
    info->type = entry[4];
    info->lba = read_u32_le(entry + 8);
    info->sectors = read_u32_le(entry + 12);
    /* 0x80 才是活动分区。类型 0 表示这一项空着。两件事互不替代。 */
    info->active = info->status == 0x80;
    info->used = info->type != 0;
}

bool mbr_parse(const unsigned char *raw, MbrImage *out) {
    int i;
    if (raw == NULL || out == NULL) {
        return false;
    }
    memcpy(out->raw, raw, MBR_SIZE);
    /*
     * 引导扇区标志是字节序列 55 AA。
     * 不要把数值 0x55AA 按小端写出去，那样会变成 AA 55，BIOS 不认。
     */
    out->sig_ok = out->raw[MBR_SIG_OFFSET] == 0x55 && out->raw[MBR_SIG_OFFSET + 1] == 0xaa;
    for (i = 0; i < MBR_ENTRY_COUNT; i++) {
        read_entry(out->raw + MBR_TABLE_OFFSET + i * MBR_ENTRY_SIZE, &out->entry[i]);
    }
    return true;
}

bool mbr_build(const PartSpec *parts, int n, unsigned char sig0, unsigned char sig1,
               MbrImage *out) {
    unsigned char raw[MBR_SIZE];
    int i;
    if (out == NULL || n < 0 || n > MBR_ENTRY_COUNT || (n > 0 && parts == NULL)) {
        return false;
    }
    memset(raw, 0, sizeof raw);
    for (i = 0; i < n; i++) {
        write_entry(raw + MBR_TABLE_OFFSET + i * MBR_ENTRY_SIZE, &parts[i]);
    }
    raw[MBR_SIG_OFFSET] = sig0;
    raw[MBR_SIG_OFFSET + 1] = sig1;
    return mbr_parse(raw, out);
}
