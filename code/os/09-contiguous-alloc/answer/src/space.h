#ifndef SPACE_H
#define SPACE_H

#define BLOCK_MAX 64

typedef enum {
    ALGO_FF = 0,
    ALGO_BF,
    ALGO_WF,
    ALGO_NF
} Algo;

typedef struct {
    long long start;
    long long size;
} Hole;

typedef struct {
    long long start;
    long long size;
    char name[16];
} Job;

typedef struct {
    int ok;
    long long at;
    long long from;
    long long hole;
    long long left;
    long long free_sum;
    long long largest;
    long long external;
} AllocInfo;

/* merge：0 两侧都不相邻，1 只与低地址相邻，2 只与高地址相邻，3 两侧都相邻。 */
typedef struct {
    int merge;
    long long at;
    long long size;
    long long into;
    long long into_size;
} FreeInfo;

typedef struct {
    char name[16];
    long long old_at;
    long long size;
    long long new_at;
} Move;

/*
 * 空闲块按起始地址升序排，方便找前后邻居。
 * 最佳、最坏适应的「链序」是显示和选择时另排的，不改这张表。
 * cursor 只给邻近适应用：下一次从这块开始，它自己还要再比一次大小。
 */
typedef struct {
    Algo algo;
    int has_mem;
    long long mem;
    int bind_dynamic;
    Hole holes[BLOCK_MAX];
    int nh;
    Job jobs[BLOCK_MAX];
    int nj;
    long long cursor;
    int cursor_valid;
    int allocated_once;
} Space;

int algo_parse(const char *text, Algo *out);
const char *algo_name(Algo algo);
const char *algo_order(Algo algo);

void space_begin(Space *s, Algo algo, int dynamic);
int space_memory(Space *s, Algo algo, long long cap, int dynamic);
void space_set_bind(Space *s, int dynamic);

int space_add_hole(Space *s, long long start, long long size);
int space_alloc(Space *s, const char *name, long long need, AllocInfo *out);
int space_free_name(Space *s, const char *name, FreeInfo *out);
int space_release(Space *s, long long start, long long size, FreeInfo *out);
int space_compact(Space *s, Move *moves, int cap, int *nmove, long long *free_at,
                  long long *free_size);

int space_chain(const Space *s, Hole *out, int cap);
int space_jobs(const Space *s, Job *out, int cap);
void space_stat(const Space *s, long long *sum, long long *largest);
int space_cursor(const Space *s, long long *out);

#endif
