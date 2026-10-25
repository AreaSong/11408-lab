#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    double rev_s;
    double seek_s;
    double rot_s;
    double xfer_s;
    double ctrl_s;
    double total_s;
} DiskTime;

void disk_sector_time(double rpm, double seek_s, double sectors, DiskTime *out);
void disk_read_time(double rpm, double seek_s, double bytes, double rate, double ctrl_s, DiskTime *out);
uint64_t disk_capacity(uint64_t surfaces, uint64_t tracks, uint64_t sectors, uint64_t sector_bytes);
double disk_media_rate(double rpm, uint64_t sectors, uint64_t sector_bytes);

int show_sector(FILE *out, double rpm, double seek_s, double sectors);
int show_read(FILE *out, double rpm, double seek_s, double rate, double bytes, double ctrl_s,
              double decimal_bytes);
int show_cap(FILE *out, uint64_t surfaces, uint64_t tracks, uint64_t sectors, uint64_t sector_bytes);
int show_media(FILE *out, double rpm, uint64_t sectors, uint64_t sector_bytes);

#endif
