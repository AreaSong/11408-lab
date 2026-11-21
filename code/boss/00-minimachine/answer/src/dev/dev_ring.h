#ifndef DEV_RING_H
#define DEV_RING_H

#include <stdio.h>

/* 循环缓冲区。满了就拒绝，空了就取不到。队头取出后腾出的位置可以再放入。 */
typedef struct DevRing DevRing;

DevRing *dev_ring_new(int cap, FILE *out);
void dev_ring_free(DevRing *r);
int dev_ring_put(DevRing *r, const char *data);
int dev_ring_get(DevRing *r);
void dev_ring_show(const DevRing *r);
int dev_ring_count(const DevRing *r);

/* 缓冲池：用完归还，下一次再借同一块。id 从 0 起。 */
typedef struct DevPool DevPool;

DevPool *dev_pool_new(int cap, FILE *out);
void dev_pool_free(DevPool *p);
int dev_pool_get(DevPool *p);
int dev_pool_put(DevPool *p, int id);
int dev_pool_free_count(const DevPool *p);

#endif
