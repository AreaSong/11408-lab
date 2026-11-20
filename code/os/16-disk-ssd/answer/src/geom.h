#ifndef GEOM_H
#define GEOM_H

long long disk_bytes(int cylinders, int surfaces, int sectors, int sec_bytes);

/* 簇号、柱面、磁头、扇区都从 0 起。扇区号是这个簇的第一个扇区。 */
int cluster_chs(int surfaces, int sectors, int cluster_secs, int cluster,
                int *cyl, int *head, int *sector);

int cluster_sstf(int surfaces, int sectors, int cluster_secs, int head_cyl,
                 const int *clusters, int n, int *order);

#endif
