#ifndef SIM_H
#define SIM_H

#define FRAME_MAX 8
#define REF_MAX 48
#define PAGE_MAX 1000000

/* 一次访问。write 为 1 是写，改进型 CLOCK 用它置修改位。 */
typedef struct {
    int page;
    int write;
} Access;

typedef struct {
    int page;
    int write;
    int fault;
    int victim; /* 没有淘汰时为 -1。最初装入空页框也是缺页，但 victim 仍是 -1 */
    int passes; /* 改进型 CLOCK 扫了几轮才找到牺牲页；没扫是 0 */
    int hand;   /* CLOCK 指针当前指着的页；没有指针时为 -1 */
    int slots[FRAME_MAX];
    int refbit[FRAME_MAX];
    int dirty[FRAME_MAX];
    int order[FRAME_MAX];
    int norder;
} Step;

typedef struct {
    char algo[8];
    int frames;
    int n;
    int faults;
    int replaces;
    Step step[REF_MAX];
} Report;

int algo_id(const char *name);
const char *algo_name(int id);

/* warm 从最久未使用排到最近使用，不计入后面的访问串，也不算缺页。
 * 返回 0；-1 算法名不对；-2 页框或访问串不合法；-3 预装的页有重复。 */
int sim_exec(const char *algo, int frames, const Access *warm, int nwarm,
             const Access *refs, int nrefs, Report *out);

/* 环上已经放好 n 页，指针在第一页。再访问 next 一次。 */
int clock_preset(const char *algo, const int *page, const int *aref, const int *dirty,
                 int n, Access next, Report *out);

int clock_ring_ok(void);

void step_blank(Step *s, int frames);

#endif
