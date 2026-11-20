#include "geom.h"

#include "sched.h"

long long disk_bytes(int cylinders, int surfaces, int sectors, int sec_bytes) {
    if (cylinders <= 0 || surfaces <= 0 || sectors <= 0 || sec_bytes <= 0) {
        return -1;
    }
    return (long long)cylinders * surfaces * sectors * sec_bytes;
}

static int per_track(int sectors, int cluster_secs) {
    if (cluster_secs <= 0 || sectors <= 0 || sectors % cluster_secs != 0) {
        return -1;
    }
    return sectors / cluster_secs;
}

int cluster_chs(int surfaces, int sectors, int cluster_secs, int cluster, int *cyl, int *head,
                int *sector) {
    int clusters_on_track;
    int clusters_on_cyl;
    int rem;

    clusters_on_track = per_track(sectors, cluster_secs);
    if (clusters_on_track < 0 || surfaces <= 0 || cluster < 0) {
        return -1;
    }
    clusters_on_cyl = surfaces * clusters_on_track;
    *cyl = cluster / clusters_on_cyl;
    rem = cluster % clusters_on_cyl;
    *head = rem / clusters_on_track;
    *sector = (rem % clusters_on_track) * cluster_secs;
    return 0;
}

int cluster_sstf(int surfaces, int sectors, int cluster_secs, int head_cyl, const int *clusters,
                 int n, int *order) {
    int cyl[SCHED_N];
    SchedOut out;
    int i;

    if (n < 1 || n > SCHED_N || head_cyl < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        int head;
        int sector;

        if (cluster_chs(surfaces, sectors, cluster_secs, clusters[i], &cyl[i], &head, &sector) !=
            0) {
            return -1;
        }
    }
    if (sched_run("sstf", head_cyl, 0, 0, 0, cyl, n, &out) != 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        order[i] = clusters[out.index[i]];
    }
    return 0;
}
