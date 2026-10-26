#ifndef SEG_H
#define SEG_H

enum { SEG_OK = 0, SEG_BOUND = 1, SEG_BOUND_NO = 2, SEG_BOUND_OFF = 3 };
enum { SP_BOUND = 0, SP_FAULT = 1, SP_TLB = 2, SP_PT = 3 };

typedef struct {
    int result;
    unsigned long long pa;
    int visits;
} SegPageInfo;

int seg_translate(unsigned long long base, unsigned long long limit, unsigned long long offset,
                  int *result, unsigned long long *pa);
int seg_len(int addr_bits, int seg_bits, unsigned long long *segments, unsigned long long *max_len);
int seg_check(unsigned long long seg_count, unsigned long long seg, unsigned long long limit,
              unsigned long long offset, unsigned long long base, int *result, unsigned long long *pa);
int seg_page(unsigned long long page_bytes, unsigned long long pages, unsigned long long page,
             unsigned long long offset, int valid, unsigned long long frame, int tlb_hit, SegPageInfo *info);

#endif
