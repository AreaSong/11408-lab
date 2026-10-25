#include <stdio.h>
#include <stdlib.h>

#include "mbr_image.h"
#include "prompt.h"

static int read_partition(int index, PartSpec *spec) {
    unsigned status = 0;
    unsigned type = 0;
    unsigned long lba = 0;
    unsigned long sectors = 0;
    prompt("请输入第 %d 个分区的状态、类型、起始LBA、扇区数：", index + 1);
    if (scanf("%x %x %lu %lu", &status, &type, &lba, &sectors) != 4) {
        return -1;
    }
    if (status > 0xffu || type > 0xffu || lba > 0xfffffffful || sectors > 0xfffffffful) {
        return -1;
    }
    spec->status = (unsigned char)status;
    spec->type = (unsigned char)type;
    spec->lba = (uint32_t)lba;
    spec->sectors = (uint32_t)sectors;
    return 0;
}

static void print_image(const MbrImage *image) {
    int i;
    int active = 0;
    printf("bytes=%d\n", MBR_SIZE);
    printf("code=%d\n", MBR_CODE_SIZE);
    printf("table=%d\n", MBR_ENTRY_SIZE * MBR_ENTRY_COUNT);
    printf("signature=%02x%02x ok=%s\n", image->raw[MBR_SIG_OFFSET], image->raw[MBR_SIG_OFFSET + 1],
           image->sig_ok ? "yes" : "no");
    for (i = 0; i < MBR_ENTRY_COUNT; i++) {
        const PartInfo *entry = &image->entry[i];
        printf("entry%d status=%02x type=%02x lba=%lu sectors=%lu active=%s used=%s\n", i,
               (unsigned)entry->status, (unsigned)entry->type, (unsigned long)entry->lba,
               (unsigned long)entry->sectors, entry->active ? "yes" : "no",
               entry->used ? "yes" : "no");
        active += entry->active ? 1 : 0;
    }
    printf("active-count=%d\n", active);
}

static int save_image(const char *path, const MbrImage *image) {
    FILE *file = fopen(path, "wb");
    size_t wrote;
    if (file == NULL) {
        fprintf(stderr, "打不开镜像文件\n");
        return -1;
    }
    wrote = fwrite(image->raw, 1, MBR_SIZE, file);
    fclose(file);
    if (wrote != MBR_SIZE) {
        fprintf(stderr, "镜像没有写全\n");
        return -1;
    }
    fprintf(stderr, "wrote %d bytes to %s\n", MBR_SIZE, path);
    return 0;
}

static int read_request(PartSpec *parts, int *count, unsigned char *sig0, unsigned char *sig1) {
    int n = 0;
    unsigned first = 0;
    unsigned second = 0;
    int i;
    prompt("请输入分区个数（0-4）：");
    if (scanf("%d", &n) != 1 || n < 0 || n > MBR_ENTRY_COUNT) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (read_partition(i, &parts[i]) != 0) {
            return -1;
        }
    }
    prompt("请输入结束标志的两个字节（如 55 aa）：");
    if (scanf("%x %x", &first, &second) != 2 || first > 0xffu || second > 0xffu) {
        return -1;
    }
    *count = n;
    *sig0 = (unsigned char)first;
    *sig1 = (unsigned char)second;
    return 0;
}

int main(int argc, char **argv) {
    PartSpec parts[MBR_ENTRY_COUNT];
    unsigned char sig0 = 0;
    unsigned char sig1 = 0;
    int count = 0;
    MbrImage image;
    if (argc > 2) {
        fprintf(stderr, "用法：分区描述从标准输入读，后面最多跟一个镜像路径\n");
        return 1;
    }
    if (read_request(parts, &count, &sig0, &sig1) != 0) {
        printf("error=format\n");
        return 1;
    }
    if (!mbr_build(parts, count, sig0, sig1, &image)) {
        printf("error=build\n");
        return 1;
    }
    print_image(&image);
    if (argc == 2 && save_image(argv[1], &image) != 0) {
        return 1;
    }
    return 0;
}
