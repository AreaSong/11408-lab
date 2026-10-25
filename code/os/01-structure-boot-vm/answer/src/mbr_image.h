#ifndef MBR_IMAGE_H
#define MBR_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#define MBR_SIZE 512
#define MBR_CODE_SIZE 446
#define MBR_ENTRY_SIZE 16
#define MBR_ENTRY_COUNT 4
#define MBR_TABLE_OFFSET 446
#define MBR_SIG_OFFSET 510

typedef struct {
    unsigned char status;
    unsigned char type;
    uint32_t lba;
    uint32_t sectors;
} PartSpec;

typedef struct {
    unsigned char status;
    unsigned char type;
    uint32_t lba;
    uint32_t sectors;
    bool active;
    bool used;
} PartInfo;

typedef struct {
    unsigned char raw[MBR_SIZE];
    bool sig_ok;
    PartInfo entry[MBR_ENTRY_COUNT];
} MbrImage;

bool mbr_build(const PartSpec *parts, int n, unsigned char sig0, unsigned char sig1,
               MbrImage *out);
bool mbr_parse(const unsigned char *raw, MbrImage *out);

#endif
